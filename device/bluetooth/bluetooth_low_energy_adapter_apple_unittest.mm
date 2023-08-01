// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_adapter_apple.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
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

#if BUILDFLAG(IS_IOS)
#include "device/bluetooth/bluetooth_adapter_ios.h"
#else
#include "device/bluetooth/bluetooth_adapter_mac.h"
#endif

namespace {

const char kTestPropertyListFileName[] = "test_property_list_file.plist";

// |kTestHashAddress| is the hash corresponding to identifier |kTestNSUUID|.
const char kTestNSUUID[] = "00000000-1111-2222-3333-444444444444";
const char kTestHashAddress[] = "D1:6F:E3:22:FD:5B";
const int kTestRssi = 0;

NSDictionary* TestPropertyListData() {
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

#if BUILDFLAG(IS_MAC)
bool IsTestDeviceSystemPaired(const std::string& address) {
  return true;
}
#endif

}  // namespace

namespace device {

class BluetoothLowEnergyAdapterAppleTest : public testing::Test {
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
      ASSERT_TRUE(base::WriteFile(path, changed_file_content));
      OnPropertyListFileChangedOnFileThread(path, false /* error */);
    }

   private:
    ~FakeBluetoothLowEnergyDeviceWatcherMac() override = default;

    void Init() override { ReadBluetoothPropertyListFile(); }

    void ReadBluetoothPropertyListFile() override {
      low_energy_device_list_updated_callback().Run(
          ParseBluetoothDevicePropertyListData(TestPropertyListData()));
    }

    base::WeakPtrFactory<FakeBluetoothLowEnergyDeviceWatcherMac>
        weak_ptr_factory_;
  };

  BluetoothLowEnergyAdapterAppleTest()
      : ui_task_runner_(new base::TestSimpleTaskRunner()),
        adapter_(CreateBluetoothLowEnergyAdapterApple()),
        adapter_low_energy_(
            static_cast<BluetoothLowEnergyAdapterApple*>(adapter_.get())),
        observer_(adapter_),
        callback_count_(0),
        error_callback_count_(0) {
    adapter_low_energy_->InitForTest(ui_task_runner_);
    fake_low_energy_device_watcher_ =
        base::MakeRefCounted<FakeBluetoothLowEnergyDeviceWatcherMac>(
            ui_task_runner_,
            base::BindRepeating(
                &BluetoothLowEnergyAdapterApple::UpdateKnownLowEnergyDevices,
                adapter_low_energy_->GetLowEnergyWeakPtr()));
#if BUILDFLAG(IS_MAC)
    static_cast<BluetoothAdapterMac*>(adapter_low_energy_)
        ->SetGetDevicePairedStatusCallbackForTesting(
            base::BindRepeating(&IsTestDeviceSystemPaired));
#endif
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_property_list_file_path_ =
        temp_dir_.GetPath().AppendASCII(kTestPropertyListFileName);
  }

  void TearDown() override { task_environment_.RunUntilIdle(); }

  void LowEnergyDeviceUpdated(CBPeripheral* peripheral,
                              NSDictionary* advertisement_data,
                              int rssi) {
    adapter_low_energy_->LowEnergyDeviceUpdated(peripheral, advertisement_data,
                                                rssi);
  }

  BluetoothLowEnergyAdapterApple* CreateBluetoothLowEnergyAdapterApple() {
#if BUILDFLAG(IS_IOS)
    return static_cast<BluetoothLowEnergyAdapterApple*>(
        new BluetoothAdapterIOS());
#else
    return static_cast<BluetoothLowEnergyAdapterApple*>(
        new BluetoothAdapterMac());
#endif
  }

  BluetoothDevice* GetDevice(const std::string& address) {
    return adapter_->GetDevice(address);
  }

  CBPeripheral* CreateMockPeripheral(const char* identifier) {
    MockCBPeripheral* mock_peripheral =
        [[MockCBPeripheral alloc] initWithUTF8StringIdentifier:identifier];
    return [mock_peripheral peripheral];
  }

  NSDictionary* AdvertisementData() {
    return @{
      CBAdvertisementDataIsConnectable : @YES,
      CBAdvertisementDataServiceDataKey : @{},
    };
  }

  std::string GetHashAddress(CBPeripheral* peripheral) {
    return BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(peripheral);
  }

  int NumDevices() { return adapter_low_energy_->devices_.size(); }

  bool DevicePresent(CBPeripheral* peripheral) {
    BluetoothDevice* device = adapter_low_energy_->GetDevice(
        BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(peripheral));
    return device;
  }

  bool SetMockCentralManager(CBManagerState desired_state) {
    mock_central_manager_ = [[MockCentralManager alloc] init];
    mock_central_manager_.state = desired_state;
    CBCentralManager* centralManager =
        static_cast<CBCentralManager*>(mock_central_manager_);
    adapter_low_energy_->SetCentralManagerForTesting(centralManager);
    return true;
  }

  void OnStartDiscoverySessionSuccess(
      std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
    active_sessions_.push_back(std::move(discovery_session));
    Callback();
  }

  int NumDiscoverySessions() {
    return adapter_low_energy_->NumDiscoverySessions();
  }

  void SetFakeLowEnergyDeviceWatcher() {
    adapter_low_energy_->SetLowEnergyDeviceWatcherForTesting(
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
  raw_ptr<BluetoothLowEnergyAdapterApple> adapter_low_energy_;
  scoped_refptr<FakeBluetoothLowEnergyDeviceWatcherMac>
      fake_low_energy_device_watcher_;
  TestBluetoothAdapterObserver observer_;
  std::vector<std::unique_ptr<BluetoothDiscoverySession>> active_sessions_;

  // Owned by |adapter_low_energy_|.
  MockCentralManager* __strong mock_central_manager_;

  int callback_count_;
  int error_callback_count_;
  base::ScopedTempDir temp_dir_;
  base::FilePath test_property_list_file_path_;
};

TEST_F(BluetoothLowEnergyAdapterAppleTest,
       AddDiscoverySessionWithLowEnergyFilter) {
  if (!SetMockCentralManager(CBManagerStatePoweredOn)) {
    return;
  }
  EXPECT_EQ(0, [mock_central_manager_ scanForPeripheralsCallCount]);
  EXPECT_EQ(0, NumDiscoverySessions());

  std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter(
      new BluetoothDiscoveryFilter(BLUETOOTH_TRANSPORT_LE));

  adapter_low_energy_->StartDiscoverySessionWithFilter(
      std::move(discovery_filter),
      /*client_name=*/std::string(),
      base::BindOnce(
          &BluetoothLowEnergyAdapterAppleTest::OnStartDiscoverySessionSuccess,
          base::Unretained(this)),
      base::BindOnce(&BluetoothLowEnergyAdapterAppleTest::ErrorCallback,
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
TEST_F(BluetoothLowEnergyAdapterAppleTest,
       AddSecondDiscoverySessionWithLowEnergyFilter) {
  if (!SetMockCentralManager(CBManagerStatePoweredOn)) {
    return;
  }
  std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter(
      new BluetoothDiscoveryFilter(BLUETOOTH_TRANSPORT_LE));
  std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter2(
      new BluetoothDiscoveryFilter(BLUETOOTH_TRANSPORT_LE));
  // Adding uuid to first discovery session so that there is a change to be made
  // when starting the second session.
  BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
  device_filter.uuids.insert(device::BluetoothUUID("1000"));
  discovery_filter->AddDeviceFilter(device_filter);
  adapter_low_energy_->StartDiscoverySessionWithFilter(
      std::move(discovery_filter),
      /*client_name=*/std::string(),
      base::BindOnce(
          &BluetoothLowEnergyAdapterAppleTest::OnStartDiscoverySessionSuccess,
          base::Unretained(this)),
      base::BindOnce(&BluetoothLowEnergyAdapterAppleTest::ErrorCallback,
                     base::Unretained(this)));
  EXPECT_TRUE(ui_task_runner_->HasPendingTask());
  ui_task_runner_->RunPendingTasks();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, NumDiscoverySessions());

  // We replaced the success callback handed to AddDiscoverySession, so
  // |adapter_low_energy_| should remain in a discovering state indefinitely.
  EXPECT_TRUE(adapter_low_energy_->IsDiscovering());

  adapter_low_energy_->StartDiscoverySessionWithFilter(
      std::move(discovery_filter2),
      /*client_name=*/std::string(),
      base::BindOnce(
          &BluetoothLowEnergyAdapterAppleTest::OnStartDiscoverySessionSuccess,
          base::Unretained(this)),
      base::BindOnce(&BluetoothLowEnergyAdapterAppleTest::ErrorCallback,
                     base::Unretained(this)));
  EXPECT_TRUE(ui_task_runner_->HasPendingTask());
  ui_task_runner_->RunPendingTasks();
  EXPECT_EQ(2, [mock_central_manager_ scanForPeripheralsCallCount]);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(2, NumDiscoverySessions());
}

TEST_F(BluetoothLowEnergyAdapterAppleTest,
       RemoveDiscoverySessionWithLowEnergyFilter) {
  if (!SetMockCentralManager(CBManagerStatePoweredOn)) {
    return;
  }
  EXPECT_EQ(0, [mock_central_manager_ scanForPeripheralsCallCount]);

  std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter(
      new BluetoothDiscoveryFilter(BLUETOOTH_TRANSPORT_LE));
  adapter_low_energy_->StartDiscoverySessionWithFilter(
      std::move(discovery_filter),
      /*client_name=*/std::string(),
      base::BindOnce(
          &BluetoothLowEnergyAdapterAppleTest::OnStartDiscoverySessionSuccess,
          base::Unretained(this)),
      base::BindOnce(&BluetoothLowEnergyAdapterAppleTest::ErrorCallback,
                     base::Unretained(this)));
  EXPECT_TRUE(ui_task_runner_->HasPendingTask());
  ui_task_runner_->RunPendingTasks();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, NumDiscoverySessions());

  EXPECT_EQ(0, [mock_central_manager_ stopScanCallCount]);
  active_sessions_[0]->Stop(
      base::BindOnce(&BluetoothLowEnergyAdapterAppleTest::Callback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothLowEnergyAdapterAppleTest::ErrorCallback,
                     base::Unretained(this)));
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(0, NumDiscoverySessions());

  // Check that removing the discovery session resulted in stopScan being called
  // on the Central Manager.
  EXPECT_EQ(1, [mock_central_manager_ stopScanCallCount]);
}

TEST_F(BluetoothLowEnergyAdapterAppleTest, CheckGetPeripheralHashAddress) {
  if (!SetMockCentralManager(CBManagerStatePoweredOn)) {
    return;
  }
  CBPeripheral* mock_peripheral = CreateMockPeripheral(kTestNSUUID);
  if (!mock_peripheral) {
    return;
  }
  EXPECT_EQ(kTestHashAddress, GetHashAddress(mock_peripheral));
}

TEST_F(BluetoothLowEnergyAdapterAppleTest, LowEnergyDeviceUpdatedNewDevice) {
  if (!SetMockCentralManager(CBManagerStatePoweredOn)) {
    return;
  }
  CBPeripheral* mock_peripheral = CreateMockPeripheral(kTestNSUUID);
  if (!mock_peripheral) {
    return;
  }
  NSDictionary* advertisement_data = AdvertisementData();

  EXPECT_EQ(0, NumDevices());
  EXPECT_FALSE(DevicePresent(mock_peripheral));
  LowEnergyDeviceUpdated(mock_peripheral, advertisement_data, kTestRssi);
  EXPECT_EQ(1, NumDevices());
  EXPECT_TRUE(DevicePresent(mock_peripheral));
}

TEST_F(BluetoothLowEnergyAdapterAppleTest, GetSystemPairedLowEnergyDevice) {
  SetFakeLowEnergyDeviceWatcher();
  ui_task_runner_->RunUntilIdle();
  EXPECT_TRUE(
      adapter_low_energy_->IsBluetoothLowEnergyDeviceSystemPaired(kTestNSUUID));
}

TEST_F(BluetoothLowEnergyAdapterAppleTest, GetNewlyPairedLowEnergyDevice) {
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

  ASSERT_TRUE(SetMockCentralManager(CBManagerStatePoweredOn));

  CBPeripheral* mock_peripheral_one = CreateMockPeripheral(kTestNSUUID);
  ASSERT_TRUE(mock_peripheral_one);

  LowEnergyDeviceUpdated(mock_peripheral_one, AdvertisementData(), kTestRssi);

  CBPeripheral* mock_peripheral_two =
      CreateMockPeripheral(kTestAddedDeviceNSUUID);
  ASSERT_TRUE(mock_peripheral_two);

  LowEnergyDeviceUpdated(mock_peripheral_two, AdvertisementData(), kTestRssi);
  observer_.Reset();

  // BluetoothAdapterMac only notifies observers of changed devices detected by
  // BluetoothLowEnergyDeviceWatcherMac if the device has been already known to
  // the system(i.e. the changed device is in BluetoothAdapter::devices_). As
  // so, add mock devices prior to setting BluetoothLowEnergyDeviceWatcherMac.
  SetFakeLowEnergyDeviceWatcher();

  EXPECT_EQ(1, observer_.device_changed_count());
  observer_.Reset();

  fake_low_energy_device_watcher_->SimulatePropertyListFileChanged(
      test_property_list_file_path_, kPropertyListFileContentWithAddedDevice);
  ui_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, observer_.device_changed_count());
  EXPECT_TRUE(adapter_low_energy_->IsBluetoothLowEnergyDeviceSystemPaired(
      kTestAddedDeviceNSUUID));
}

TEST_F(BluetoothLowEnergyAdapterAppleTest, NotifyObserverWhenDeviceIsUnpaired) {
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

  if (!SetMockCentralManager(CBManagerStatePoweredOn)) {
    return;
  }

  CBPeripheral* mock_peripheral = CreateMockPeripheral(kTestNSUUID);
  if (!mock_peripheral) {
    return;
  }

  LowEnergyDeviceUpdated(mock_peripheral, AdvertisementData(), kTestRssi);
  observer_.Reset();

  SetFakeLowEnergyDeviceWatcher();
  EXPECT_EQ(1, observer_.device_changed_count());
  observer_.Reset();

  fake_low_energy_device_watcher_->SimulatePropertyListFileChanged(
      test_property_list_file_path_, kPropertyListFileContentWithRemovedDevice);
  ui_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, observer_.device_changed_count());
  EXPECT_FALSE(
      adapter_low_energy_->IsBluetoothLowEnergyDeviceSystemPaired(kTestNSUUID));
}

}  // namespace device
