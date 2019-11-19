// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_mac.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#import "device/bluetooth/bluetooth_low_energy_device_mac.h"
#include "device/bluetooth/bluetooth_low_energy_device_watcher_mac.h"
#import "device/bluetooth/test/mock_bluetooth_cbperipheral_mac.h"
#import "device/bluetooth/test/mock_bluetooth_central_manager_mac.h"
#import "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_IOS)
#import <CoreBluetooth/CoreBluetooth.h>
#else  // !defined(OS_IOS)
#import <IOBluetooth/IOBluetooth.h>
#endif  // defined(OS_IOS)

// List of undocumented IOBluetooth APIs used for BluetoothAdapterMac.
extern "C" {
int IOBluetoothPreferenceGetControllerPowerState();
void IOBluetoothPreferenceSetControllerPowerState(int state);
}

namespace {

const char kTestPropertyListFileName[] = "test_property_list_file.plist";

// |kTestHashAddress| is the hash corresponding to identifier |kTestNSUUID|.
const char kTestNSUUID[] = "00000000-1111-2222-3333-444444444444";
const char kTestHashAddress[] = "D1:6F:E3:22:FD:5B";
const int kTestRssi = 0;

NSDictionary* CreateTestPropertyListData() {
  return @{
    @"CoreBluetoothCache" : @{
      @"00000000-1111-2222-3333-444444444444" : @{
        @"DeviceAddress" : @"22-22-22-22-22-22",
        @"DeviceAddressType" : @1,
        @"ServiceChangedHandle" : @3,
        @"ServiceChangeSubscribed" : @0,
        @"ServiceDiscoveryComplete" : @0
      }
    }
  };
}

bool IsTestDeviceSystemPaired(const std::string& address) {
  return true;
}

}  // namespace

namespace device {

class BluetoothAdapterMacTest : public testing::Test {
 public:
  class FakeBluetoothLowEnergyDeviceWatcherMac
      : public BluetoothLowEnergyDeviceWatcherMac {
   public:
    FakeBluetoothLowEnergyDeviceWatcherMac(
        scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner,
        LowEnergyDeviceListUpdatedCallback callback)
        : BluetoothLowEnergyDeviceWatcherMac(ui_thread_task_runner, callback),
          weak_ptr_factory_(this) {}

    void SimulatePropertyListFileChanged(
        const base::FilePath& path,
        const std::string& changed_file_content) {
      auto expected_file_size = changed_file_content.length();
      ASSERT_EQ(static_cast<int>(expected_file_size),
                base::WriteFile(path, changed_file_content.data(),
                                expected_file_size));
      OnPropertyListFileChangedOnFileThread(path, false /* error */);
    }

   private:
    ~FakeBluetoothLowEnergyDeviceWatcherMac() override = default;

    void Init() override { ReadBluetoothPropertyListFile(); }

    void ReadBluetoothPropertyListFile() override {
      low_energy_device_list_updated_callback().Run(
          ParseBluetoothDevicePropertyListData(CreateTestPropertyListData()));
    }

    base::WeakPtrFactory<FakeBluetoothLowEnergyDeviceWatcherMac>
        weak_ptr_factory_;
  };

  BluetoothAdapterMacTest()
      : ui_task_runner_(new base::TestSimpleTaskRunner()),
        adapter_(new BluetoothAdapterMac()),
        adapter_mac_(static_cast<BluetoothAdapterMac*>(adapter_.get())),
        observer_(adapter_),
        callback_count_(0),
        error_callback_count_(0) {
    adapter_mac_->SetGetDevicePairedStatusCallbackForTesting(
        base::BindRepeating(&IsTestDeviceSystemPaired));
    adapter_mac_->InitForTest(ui_task_runner_);
    fake_low_energy_device_watcher_ =
        base::MakeRefCounted<FakeBluetoothLowEnergyDeviceWatcherMac>(
            ui_task_runner_,
            base::BindRepeating(
                &BluetoothAdapterMac::UpdateKnownLowEnergyDevices,
                adapter_mac_->weak_ptr_factory_.GetWeakPtr()));
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_property_list_file_path_ =
        temp_dir_.GetPath().AppendASCII(kTestPropertyListFileName);
  }

  void TearDown() override { task_environment_.RunUntilIdle(); }

  // Helper methods for setup and access to BluetoothAdapterMacTest's members.
  void PollAdapter() { adapter_mac_->PollAdapter(); }

  void SetHostControllerPowerFunction(bool powered) {
    adapter_mac_->SetHostControllerStateFunctionForTesting(
        base::BindLambdaForTesting([powered] {
          BluetoothAdapterMac::HostControllerState state;
          state.classic_powered = powered;
          return state;
        }));
  }

  void LowEnergyDeviceUpdated(CBPeripheral* peripheral,
                              NSDictionary* advertisement_data,
                              int rssi) {
    adapter_mac_->LowEnergyDeviceUpdated(peripheral, advertisement_data, rssi);
  }

  BluetoothDevice* GetDevice(const std::string& address) {
    return adapter_->GetDevice(address);
  }

  CBPeripheral* CreateMockPeripheral(const char* identifier) {
    base::scoped_nsobject<MockCBPeripheral> mock_peripheral(
        [[MockCBPeripheral alloc] initWithUTF8StringIdentifier:identifier]);
    return [[mock_peripheral peripheral] retain];
  }

  NSDictionary* AdvertisementData() {
    NSDictionary* advertisement_data = @{
      CBAdvertisementDataIsConnectable : @(YES),
      CBAdvertisementDataServiceDataKey : [NSDictionary dictionary],
    };
    return [advertisement_data retain];
  }

  std::string GetHashAddress(CBPeripheral* peripheral) {
    return BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(peripheral);
  }

  int NumDevices() { return adapter_mac_->devices_.size(); }

  bool DevicePresent(CBPeripheral* peripheral) {
    BluetoothDevice* device = adapter_mac_->GetDevice(
        BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(peripheral));
    return (device != NULL);
  }

  bool SetMockCentralManager(CBCentralManagerState desired_state) {
    mock_central_manager_.reset([[MockCentralManager alloc] init]);
    [mock_central_manager_ setState:desired_state];
    CBCentralManager* centralManager =
        static_cast<CBCentralManager*>(mock_central_manager_.get());
    adapter_mac_->SetCentralManagerForTesting(centralManager);
    return true;
  }

  void OnStartDiscoverySessionSuccess(
      std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
    active_sessions_.push_back(std::move(discovery_session));
    Callback();
  }

  int NumDiscoverySessions() { return adapter_mac_->NumDiscoverySessions(); }

  void SetFakeLowEnergyDeviceWatcher() {
    adapter_mac_->SetLowEnergyDeviceWatcherForTesting(
        fake_low_energy_device_watcher_);
  }

  // Generic callbacks.
  void Callback() { ++callback_count_; }
  void ErrorCallback() { ++error_callback_count_; }
  void DiscoveryErrorCallback(UMABluetoothDiscoverySessionOutcome) {
    ++error_callback_count_;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> ui_task_runner_;
  scoped_refptr<BluetoothAdapter> adapter_;
  BluetoothAdapterMac* adapter_mac_;
  scoped_refptr<FakeBluetoothLowEnergyDeviceWatcherMac>
      fake_low_energy_device_watcher_;
  TestBluetoothAdapterObserver observer_;
  std::vector<std::unique_ptr<BluetoothDiscoverySession>> active_sessions_;

  // Owned by |adapter_mac_|.
  base::scoped_nsobject<MockCentralManager> mock_central_manager_;

  int callback_count_;
  int error_callback_count_;
  base::ScopedTempDir temp_dir_;
  base::FilePath test_property_list_file_path_;
};

// Test if private IOBluetooth APIs are callable on all supported macOS
// versions.
TEST_F(BluetoothAdapterMacTest, IOBluetoothPrivateAPIs) {
  // Obtain current power state, toggle it, and reset it to it's original value.
  int previous_state = IOBluetoothPreferenceGetControllerPowerState();
  IOBluetoothPreferenceSetControllerPowerState(!previous_state);
  IOBluetoothPreferenceSetControllerPowerState(previous_state);
}

TEST_F(BluetoothAdapterMacTest, Poll) {
  PollAdapter();
  EXPECT_TRUE(ui_task_runner_->HasPendingTask());
}

TEST_F(BluetoothAdapterMacTest, PollAndChangePower) {
  // By default the adapter is powered off, check that this expectation matches
  // reality.
  EXPECT_FALSE(adapter_mac_->IsPowered());
  EXPECT_EQ(0, observer_.powered_changed_count());

  SetHostControllerPowerFunction(true);
  PollAdapter();
  EXPECT_TRUE(ui_task_runner_->HasPendingTask());
  ui_task_runner_->RunPendingTasks();
  EXPECT_EQ(1, observer_.powered_changed_count());
  EXPECT_TRUE(observer_.last_powered());
  EXPECT_TRUE(adapter_mac_->IsPowered());

  SetHostControllerPowerFunction(false);
  PollAdapter();
  EXPECT_TRUE(ui_task_runner_->HasPendingTask());
  ui_task_runner_->RunPendingTasks();
  EXPECT_EQ(2, observer_.powered_changed_count());
  EXPECT_FALSE(observer_.last_powered());
  EXPECT_FALSE(adapter_mac_->IsPowered());
}

TEST_F(BluetoothAdapterMacTest, AddDiscoverySessionWithLowEnergyFilter) {
  if (!SetMockCentralManager(CBCentralManagerStatePoweredOn))
    return;
  EXPECT_EQ(0, [mock_central_manager_ scanForPeripheralsCallCount]);
  EXPECT_EQ(0, NumDiscoverySessions());

  std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter(
      new BluetoothDiscoveryFilter(BLUETOOTH_TRANSPORT_LE));

  adapter_mac_->StartDiscoverySessionWithFilter(
      std::move(discovery_filter),
      base::BindRepeating(
          &BluetoothAdapterMacTest::OnStartDiscoverySessionSuccess,
          base::Unretained(this)),
      base::BindRepeating(&BluetoothAdapterMacTest::ErrorCallback,
                          base::Unretained(this)));
  EXPECT_TRUE(ui_task_runner_->HasPendingTask());
  ui_task_runner_->RunPendingTasks();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, NumDiscoverySessions());

  // Check that adding a discovery session resulted in
  // scanForPeripheralsWithServices being called on the Central Manager.
  EXPECT_EQ(1, [mock_central_manager_ scanForPeripheralsCallCount]);
}

// TODO(krstnmnlsn): Test changing the filter when adding the second discovery
// session (once we have that ability).
TEST_F(BluetoothAdapterMacTest, AddSecondDiscoverySessionWithLowEnergyFilter) {
  if (!SetMockCentralManager(CBCentralManagerStatePoweredOn))
    return;
  std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter(
      new BluetoothDiscoveryFilter(BLUETOOTH_TRANSPORT_LE));
  std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter2(
      new BluetoothDiscoveryFilter(BLUETOOTH_TRANSPORT_LE));
  // Adding uuid to first discovery session so that there is a change to be made
  // when starting the second session.
  BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
  device_filter.uuids.insert(device::BluetoothUUID("1000"));
  discovery_filter->AddDeviceFilter(device_filter);
  adapter_mac_->StartDiscoverySessionWithFilter(
      std::move(discovery_filter),
      base::BindRepeating(
          &BluetoothAdapterMacTest::OnStartDiscoverySessionSuccess,
          base::Unretained(this)),
      base::BindRepeating(&BluetoothAdapterMacTest::ErrorCallback,
                          base::Unretained(this)));
  EXPECT_TRUE(ui_task_runner_->HasPendingTask());
  ui_task_runner_->RunPendingTasks();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, NumDiscoverySessions());

  // We replaced the success callback handed to AddDiscoverySession, so
  // |adapter_mac_| should remain in a discovering state indefinitely.
  EXPECT_TRUE(adapter_mac_->IsDiscovering());

  adapter_mac_->StartDiscoverySessionWithFilter(
      std::move(discovery_filter2),
      base::BindRepeating(
          &BluetoothAdapterMacTest::OnStartDiscoverySessionSuccess,
          base::Unretained(this)),
      base::BindRepeating(&BluetoothAdapterMacTest::ErrorCallback,
                          base::Unretained(this)));
  EXPECT_TRUE(ui_task_runner_->HasPendingTask());
  ui_task_runner_->RunPendingTasks();
  EXPECT_EQ(2, [mock_central_manager_ scanForPeripheralsCallCount]);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(2, NumDiscoverySessions());
}

TEST_F(BluetoothAdapterMacTest, RemoveDiscoverySessionWithLowEnergyFilter) {
  if (!SetMockCentralManager(CBCentralManagerStatePoweredOn))
    return;
  EXPECT_EQ(0, [mock_central_manager_ scanForPeripheralsCallCount]);

  std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter(
      new BluetoothDiscoveryFilter(BLUETOOTH_TRANSPORT_LE));
  adapter_mac_->StartDiscoverySessionWithFilter(
      std::move(discovery_filter),
      base::BindRepeating(
          &BluetoothAdapterMacTest::OnStartDiscoverySessionSuccess,
          base::Unretained(this)),
      base::BindRepeating(&BluetoothAdapterMacTest::ErrorCallback,
                          base::Unretained(this)));
  EXPECT_TRUE(ui_task_runner_->HasPendingTask());
  ui_task_runner_->RunPendingTasks();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, NumDiscoverySessions());

  EXPECT_EQ(0, [mock_central_manager_ stopScanCallCount]);
  active_sessions_[0]->Stop(
      base::BindRepeating(&BluetoothAdapterMacTest::Callback,
                          base::Unretained(this)),
      base::BindRepeating(&BluetoothAdapterMacTest::ErrorCallback,
                          base::Unretained(this)));
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(0, NumDiscoverySessions());

  // Check that removing the discovery session resulted in stopScan being called
  // on the Central Manager.
  EXPECT_EQ(1, [mock_central_manager_ stopScanCallCount]);
}

TEST_F(BluetoothAdapterMacTest, CheckGetPeripheralHashAddress) {
  if (!SetMockCentralManager(CBCentralManagerStatePoweredOn))
    return;
  base::scoped_nsobject<CBPeripheral> mock_peripheral(
      CreateMockPeripheral(kTestNSUUID));
  if (!mock_peripheral)
    return;
  EXPECT_EQ(kTestHashAddress, GetHashAddress(mock_peripheral));
}

TEST_F(BluetoothAdapterMacTest, LowEnergyDeviceUpdatedNewDevice) {
  if (!SetMockCentralManager(CBCentralManagerStatePoweredOn))
    return;
  base::scoped_nsobject<CBPeripheral> mock_peripheral(
      CreateMockPeripheral(kTestNSUUID));
  if (!mock_peripheral)
    return;
  base::scoped_nsobject<NSDictionary> advertisement_data(AdvertisementData());

  EXPECT_EQ(0, NumDevices());
  EXPECT_FALSE(DevicePresent(mock_peripheral));
  LowEnergyDeviceUpdated(mock_peripheral, advertisement_data, kTestRssi);
  EXPECT_EQ(1, NumDevices());
  EXPECT_TRUE(DevicePresent(mock_peripheral));
}

TEST_F(BluetoothAdapterMacTest, GetSystemPairedLowEnergyDevice) {
  SetFakeLowEnergyDeviceWatcher();
  ui_task_runner_->RunUntilIdle();
  EXPECT_TRUE(
      adapter_mac_->IsBluetoothLowEnergyDeviceSystemPaired(kTestNSUUID));
}

TEST_F(BluetoothAdapterMacTest, GetNewlyPairedLowEnergyDevice) {
  constexpr char kPropertyListFileContentWithAddedDevice[] =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
      "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
      "<plist version=\"1.0\">"
      "<dict>"
      "   <key>CoreBluetoothCache</key>"
      "     <dict> "
      "      <key>E7F8589A-A7D9-4B94-9A08-D89076A159F4</key>"
      "      <dict> "
      "         <key>DeviceAddress</key>"
      "         <string>11-11-11-11-11-11</string>"
      "         <key>DeviceAddressType</key>"
      "         <integer>1</integer>"
      "         <key>ServiceChangedHandle</key>"
      "         <integer>3</integer>"
      "         <key>ServiceChangeSubscribed</key>"
      "         <integer>0</integer>"
      "         <key>ServiceDiscoveryComplete</key>"
      "         <integer>0</integer>"
      "      </dict>"
      "      <key>00000000-1111-2222-3333-444444444444</key>"
      "      <dict> "
      "         <key>DeviceAddress</key>"
      "         <string>22-22-22-22-22-22</string>"
      "         <key>DeviceAddressType</key>"
      "         <integer>1</integer>"
      "         <key>ServiceChangedHandle</key>"
      "         <integer>3</integer>"
      "         <key>ServiceChangeSubscribed</key>"
      "         <integer>0</integer>"
      "         <key>ServiceDiscoveryComplete</key>"
      "         <integer>0</integer>"
      "      </dict>"

      "     </dict>"
      "</dict>"
      "</plist>";

  const char kTestAddedDeviceNSUUID[] = "E7F8589A-A7D9-4B94-9A08-D89076A159F4";

  ASSERT_TRUE(SetMockCentralManager(CBCentralManagerStatePoweredOn));

  base::scoped_nsobject<CBPeripheral> mock_peripheral_one(
      CreateMockPeripheral(kTestNSUUID));
  ASSERT_TRUE(mock_peripheral_one);

  LowEnergyDeviceUpdated(
      mock_peripheral_one,
      base::scoped_nsobject<NSDictionary>(AdvertisementData()), kTestRssi);

  base::scoped_nsobject<CBPeripheral> mock_peripheral_two(
      CreateMockPeripheral(kTestAddedDeviceNSUUID));
  ASSERT_TRUE(mock_peripheral_two);

  LowEnergyDeviceUpdated(
      mock_peripheral_two,
      base::scoped_nsobject<NSDictionary>(AdvertisementData()), kTestRssi);
  observer_.Reset();

  // BluetoothAdapterMac only notifies observers of changed devices detected by
  // BluetoothLowEnergyDeviceWatcherMac if the device has been already known to
  // the system(i.e. the changed device is in BluetoothAdatper::devices_). As
  // so, add mock devices prior to setting BluetoothLowenergyDeviceWatcherMac.
  SetFakeLowEnergyDeviceWatcher();

  EXPECT_EQ(1, observer_.device_changed_count());
  observer_.Reset();

  fake_low_energy_device_watcher_->SimulatePropertyListFileChanged(
      test_property_list_file_path_, kPropertyListFileContentWithAddedDevice);
  ui_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, observer_.device_changed_count());
  EXPECT_TRUE(adapter_mac_->IsBluetoothLowEnergyDeviceSystemPaired(
      kTestAddedDeviceNSUUID));
}

TEST_F(BluetoothAdapterMacTest, NotifyObserverWhenDeviceIsUnpaired) {
  constexpr char kPropertyListFileContentWithRemovedDevice[] =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
      "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
      "<plist version=\"1.0\">"
      "<dict>"
      "   <key>CoreBluetoothCache</key>"
      "     <dict> "
      "     </dict>"
      "</dict>"
      "</plist>";

  if (!SetMockCentralManager(CBCentralManagerStatePoweredOn))
    return;

  base::scoped_nsobject<CBPeripheral> mock_peripheral(
      CreateMockPeripheral(kTestNSUUID));
  if (!mock_peripheral)
    return;

  LowEnergyDeviceUpdated(
      mock_peripheral, base::scoped_nsobject<NSDictionary>(AdvertisementData()),
      kTestRssi);
  observer_.Reset();

  SetFakeLowEnergyDeviceWatcher();
  EXPECT_EQ(1, observer_.device_changed_count());
  observer_.Reset();

  fake_low_energy_device_watcher_->SimulatePropertyListFileChanged(
      test_property_list_file_path_, kPropertyListFileContentWithRemovedDevice);
  ui_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, observer_.device_changed_count());
  EXPECT_FALSE(
      adapter_mac_->IsBluetoothLowEnergyDeviceSystemPaired(kTestNSUUID));
}

}  // namespace device
