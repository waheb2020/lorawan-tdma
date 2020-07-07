/*
 * This script simulates a simple network in which one end device sends one
 * packet to the gateway.
 */

#include "ns3/end-device-lora-phy.h"
#include "ns3/gateway-lora-phy.h"
#include "ns3/end-device-lorawan-mac.h"
#include "ns3/gateway-lorawan-mac.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/lora-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/node-container.h"
#include "ns3/position-allocator.h"
#include "ns3/tdma-sender-helper.h"
#include "ns3/command-line.h"
#include <algorithm>
#include <ctime>

// Lora Energy Model
#include "ns3/basic-energy-source-helper.h"
#include "ns3/lora-radio-energy-model-helper.h"
#include "ns3/file-helper.h"
#include "ns3/names.h"

using namespace ns3;
using namespace lorawan;
//using namespace tdma;

NS_LOG_COMPONENT_DEFINE("EvaNetworkTDMAv4");

int main(int argc, char *argv[]) {
	uint16_t nDevices = 2;
	uint16_t nPeriods = 20;
	uint16_t nHours = 2;
	uint16_t interval = 60 * 30; //30 minutes by default

	CommandLine cmd;
	cmd.AddValue("nDevices", "Number of end devices", nDevices);
	cmd.AddValue("interval", "Trigger sending interval in seconds", interval);
	cmd.AddValue("nHours", "Simulation hours", nHours);
	cmd.Parse(argc, argv);

	NS_LOG_INFO("Params: " << nDevices << ", " << interval);

	// Set up logging
	LogComponentEnable("EvaNetworkTDMAv4", LOG_LEVEL_ALL);
//  LogComponentEnable ("LoraChannel", LOG_LEVEL_INFO);
	LogComponentEnable("LoraPhy", LOG_LEVEL_ALL);
//  LogComponentEnable ("EndDeviceLoraPhy", LOG_LEVEL_ALL);
	LogComponentEnable("GatewayLoraPhy", LOG_LEVEL_ALL);
//  LogComponentEnable ("LoraInterferenceHelper", LOG_LEVEL_ALL);
//  LogComponentEnable ("LorawanMac", LOG_LEVEL_ALL);
//  LogComponentEnable ("EndDeviceLorawanMac", LOG_LEVEL_ALL);
	LogComponentEnable("ClassAEndDeviceLorawanMac", LOG_LEVEL_ALL);
	LogComponentEnable("GatewayLorawanMac", LOG_LEVEL_ALL);
//  LogComponentEnable ("LogicalLoraChannelHelper", LOG_LEVEL_ALL);
//  LogComponentEnable ("LogicalLoraChannel", LOG_LEVEL_ALL);
//  LogComponentEnable ("LoraHelper", LOG_LEVEL_ALL);
//  LogComponentEnable ("LoraPhyHelper", LOG_LEVEL_ALL);
//  LogComponentEnable ("LorawanMacHelper", LOG_LEVEL_ALL);
//  LogComponentEnable ("LorawanMacHeader", LOG_LEVEL_ALL);
//  LogComponentEnable ("LoraFrameHeader", LOG_LEVEL_ALL);
//  LogComponentEnable ("Names", LOG_LEVEL_ALL);
//  LogComponentEnable ("FileHelper", LOG_LEVEL_ALL);
//  LogComponentEnable ("Config", LOG_LEVEL_ALL);
//  LogComponentEnable ("LoraPacketTracker", LOG_LEVEL_ALL);
	LogComponentEnable("TDMASenderHelper", LOG_LEVEL_ALL);
	LogComponentEnable("TDMASender", LOG_LEVEL_ALL);
	LogComponentEnableAll(LOG_PREFIX_FUNC);
	LogComponentEnableAll(LOG_PREFIX_NODE);
	LogComponentEnableAll(LOG_PREFIX_TIME);

	/************************
	 *  Create the channel  *
	 ************************/

	NS_LOG_INFO("Creating the channel...");

	// Create the lora channel object
	Ptr<LogDistancePropagationLossModel> loss = CreateObject<
			LogDistancePropagationLossModel>();
	loss->SetPathLossExponent(3.76);
	loss->SetReference(1, 7.7);

	Ptr<PropagationDelayModel> delay = CreateObject<
			ConstantSpeedPropagationDelayModel>();

	Ptr<LoraChannel> channel = CreateObject<LoraChannel>(loss, delay);

	/************************
	 *  Create the helpers  *
	 ************************/

	NS_LOG_INFO("Setting up helpers...");

	MobilityHelper mobility;
	Ptr<ListPositionAllocator> allocator =
			CreateObject<ListPositionAllocator>();
	allocator->Add(Vector(0, 0, 0));
	mobility.SetPositionAllocator(allocator);
	mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

	// Create the LoraPhyHelper
	LoraPhyHelper phyHelper = LoraPhyHelper();
	phyHelper.SetChannel(channel);

	// Create the LorawanMacHelper
	LorawanMacHelper macHelper = LorawanMacHelper();

	// Create the LoraHelper
	LoraHelper helper = LoraHelper();
	helper.EnablePacketTracking();

	/************************
	 *  Create End Devices  *
	 ************************/

	NS_LOG_INFO("Creating the end device...");

	// Create a set of nodes
	NodeContainer endDevices;
	endDevices.Create(nDevices);

	// Assign a mobility model to the node
	mobility.Install(endDevices);

	// Create the LoraNetDevices of the end devices
	phyHelper.SetDeviceType(LoraPhyHelper::ED);
	macHelper.SetDeviceType(LorawanMacHelper::ED_A);
	macHelper.SetRetransMax(0); //0=Disable
	NetDeviceContainer endDevicesNetDevices = helper.Install(phyHelper,
			macHelper, endDevices);

	/*********************
	 *  Create Gateways  *
	 *********************/

	NS_LOG_INFO("Creating the gateway...");
	NodeContainer gateways;
	gateways.Create(1);

	mobility.Install(gateways);

	// Create a netdevice for each gateway
	phyHelper.SetDeviceType(LoraPhyHelper::GW);
	macHelper.SetDeviceType(LorawanMacHelper::GW);
	helper.Install(phyHelper, macHelper, gateways);

	/*********************************************
	 *  Install applications on the end devices  *
	 *********************************************/
	TDMAParams params;
	params.interval = interval; // 3 minutes
	TDMASenderHelper senderHelper;
	senderHelper.SetTDMAParams(params);

	senderHelper.Install(endDevices);

	/******************
	 * Set Data Rates *
	 ******************/
	std::vector<int> sfQuantity(6);
	sfQuantity = macHelper.SetSpreadingFactorsUp(endDevices, gateways, channel);

	// Activate printing of ED MAC parameters
	Time stateSamplePeriod = Seconds(0.040);
	helper.EnablePeriodicDeviceStatusPrinting(endDevices, gateways,
			"nodeData.txt", stateSamplePeriod);
	helper.EnablePeriodicPhyPerformancePrinting(gateways, "phyPerformance.txt",
			stateSamplePeriod);
	helper.EnablePeriodicGlobalPerformancePrinting("globalPerformance.txt",
			stateSamplePeriod);

	LoraPacketTracker& tracker = helper.GetPacketTracker();

	/************************
	 * Install Energy Model *
	 ************************/

	BasicEnergySourceHelper basicSourceHelper;
	LoraRadioEnergyModelHelper radioEnergyHelper;

	// configure energy source
	basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ",
			DoubleValue(10000)); // Energy in J
	basicSourceHelper.Set("BasicEnergySupplyVoltageV", DoubleValue(3.3));

	radioEnergyHelper.Set("StandbyCurrentA", DoubleValue(0.0014));
	radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.028));
	radioEnergyHelper.Set("SleepCurrentA", DoubleValue(0.0000015));
	radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.0112));

	radioEnergyHelper.SetTxCurrentModel("ns3::ConstantLoraTxCurrentModel",
			"TxCurrent", DoubleValue(0.028));

	// install source on EDs' nodes
	EnergySourceContainer sources = basicSourceHelper.Install(endDevices);
	for (uint16_t i = 0; i < nDevices; i++) {
		char name[40];
		sprintf(name, "/Names/EnergySourceTDMA%04d", (i + 1));
		Names::Add(name, sources.Get(i));
	}

	// install device model
	DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(
			endDevicesNetDevices, sources);

	/**************
	 * Get output *
	 **************/

	FileHelper fileHelpers[nDevices];
	for (uint16_t i = 0; i < nDevices; i++) {
		char name[60];
		char fname[40];
		sprintf(name, "/Names/EnergySourceTDMA%04d/RemainingEnergy", (i + 1));
		sprintf(fname, "battery-level-%04d", (i + 1));
		fileHelpers[i].ConfigureFile(fname, FileAggregator::SPACE_SEPARATED);
		fileHelpers[i].WriteProbe("ns3::DoubleProbe", name, "Output");
	}

	/****************
	 *  Simulation  *
	 ****************/

	Simulator::Stop(Hours(nHours));

	Simulator::Run();

	Simulator::Destroy();

	std::cout
			<< tracker.CountMacPacketsGlobally(Seconds(1200 * (nPeriods - 2)),
					Seconds(1200 * (nPeriods - 1))) << std::endl;

	return 0;
}
