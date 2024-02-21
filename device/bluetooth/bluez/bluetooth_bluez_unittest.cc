// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/repeating_test_future.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/bluez/bluetooth_device_bluez.h"
#include "device/bluetooth/bluez/bluetooth_pairing_bluez.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_admin_policy_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_agent_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_battery_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_service_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_input_client.h"
#include "device/bluetooth/test/mock_pairing_delegate.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#include "device/bluetooth/dbus/fake_bluetooth_advertisement_monitor_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_le_advertising_manager_client.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/data_decoder/public/mojom/ble_scan_parser.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

using ::device::BluetoothAdapter;
using ::device::BluetoothAdapterFactory;
using ::device::BluetoothDevice;
using BatteryInfo = ::device::BluetoothDevice::BatteryInfo;
using BatteryType = ::device::BluetoothDevice::BatteryType;
using ::device::BluetoothDeviceType;
using ::device::BluetoothDiscoveryFilter;
using ::device::BluetoothDiscoverySession;
using ::device::BluetoothUUID;
using ::device::MockPairingDelegate;
using ::device::TestBluetoothAdapterObserver;
using ::testing::_;
using ::testing::StrictMock;

#if BUILDFLAG(IS_CHROMEOS)
// Background scanning filter values.
constexpr int16_t kBackgroundScanningDeviceFoundRSSIThreshold = -80;
constexpr int16_t kBackgroundScanningDeviceLostRSSIThreshold = -100;
constexpr base::TimeDelta kBackgroundScanningDeviceFoundTimeout =
    base::Seconds(1);
constexpr base::TimeDelta kBackgroundScanningDeviceLostTimeout =
    base::Seconds(5);
// This pattern value encodes the Fast Initiation service ID of 0xfe2c and the
// model ID of 0xfc128e.
constexpr uint8_t kBackgroundScanningFilterPatternValue[] = {0x2c, 0xfe, 0xfc,
                                                             0x12, 0x8e};
std::unique_ptr<device::BluetoothLowEnergyScanFilter>
CreateLowEnergyScanFilter() {
  auto pattern_value =
      std::vector<uint8_t>(std::begin(kBackgroundScanningFilterPatternValue),
                           std::end(kBackgroundScanningFilterPatternValue));
  device::BluetoothLowEnergyScanFilter::Pattern pattern(
      /*start_position=*/0,
      device::BluetoothLowEnergyScanFilter::AdvertisementDataType::kServiceData,
      std::move(pattern_value));
  return device::BluetoothLowEnergyScanFilter::Create(
      kBackgroundScanningDeviceFoundRSSIThreshold,
      kBackgroundScanningDeviceLostRSSIThreshold,
      kBackgroundScanningDeviceFoundTimeout,
      kBackgroundScanningDeviceLostTimeout, {pattern},
      /*rssi_sampling_period=*/std::nullopt);
}

bluez::FakeBluetoothAdvertisementMonitorApplicationServiceProvider*
GetAdvertisementMonitorApplicationManger() {
  return static_cast<bluez::FakeBluetoothAdvertisementMonitorManagerClient*>(
             bluez::BluezDBusManager::Get()
                 ->GetBluetoothAdvertisementMonitorManagerClient())
      ->application_provider();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void ScheduleAsynchronousCancelPairing(BluetoothDevice* device) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BluetoothDevice::CancelPairing,
                                base::Unretained(device)));
}

void ScheduleAsynchronousRejectPairing(BluetoothDevice* device) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BluetoothDevice::RejectPairing,
                                base::Unretained(device)));
}

}  // namespace

namespace bluez {

namespace {

bool* dump_without_crashing_flag;
extern "C" void HandleDumpWithoutCrashing() {
  *dump_without_crashing_flag = true;
}

// Callback for BluetoothDevice::GetConnectionInfo() that simply saves the
// connection info to the bound argument.
void SaveConnectionInfo(BluetoothDevice::ConnectionInfo* out,
                        const BluetoothDevice::ConnectionInfo& conn_info) {
  *out = conn_info;
}

// Find |address| in |devices|, if found returns the index otherwise returns -1.
int GetDeviceIndexByAddress(const BluetoothAdapter::DeviceList& devices,
                            const char* address) {
  int idx = -1;
  for (auto* device : devices) {
    ++idx;
    if (device->GetAddress().compare(address) == 0)
      return idx;
  }
  return -1;
}

#if BUILDFLAG(IS_CHROMEOS)
class FakeBleScanParserImpl : public data_decoder::mojom::BleScanParser {
 public:
  FakeBleScanParserImpl() = default;

  FakeBleScanParserImpl(const FakeBleScanParserImpl&) = delete;
  FakeBleScanParserImpl& operator=(const FakeBleScanParserImpl&) = delete;

  ~FakeBleScanParserImpl() override = default;

  // mojom::BleScanParser:
  void Parse(const std::vector<uint8_t>& advertisement_data,
             ParseCallback callback) override {
    std::move(callback).Run(nullptr);
  }
};
#endif  // BUILDFLAG(IS_CHROMEOS)

using MockDBusErrorCallback = base::MockCallback<
    base::OnceCallback<void(const std::string&, const std::string&)>>;

class FakeBluetoothProfileServiceProviderDelegate
    : public bluez::BluetoothProfileServiceProvider::Delegate {
 public:
  FakeBluetoothProfileServiceProviderDelegate() = default;

  // bluez::BluetoothProfileServiceProvider::Delegate:
  void Released() override {}

  void NewConnection(
      const dbus::ObjectPath&,
      base::ScopedFD,
      const bluez::BluetoothProfileServiceProvider::Delegate::Options&,
      ConfirmationCallback) override {}

  void RequestDisconnection(const dbus::ObjectPath&,
                            ConfirmationCallback) override {}

  void Cancel() override {}
};

#if BUILDFLAG(IS_CHROMEOS)
class FakeBluetoothLowEnergyScanSessionDelegate
    : public device::BluetoothLowEnergyScanSession::Delegate {
 public:
  FakeBluetoothLowEnergyScanSessionDelegate() = default;

  // device::BluetoothLowEnergyScanSession::Delegate
  void OnSessionStarted(
      device::BluetoothLowEnergyScanSession* scan_session,
      std::optional<device::BluetoothLowEnergyScanSession::ErrorCode>
          error_code) override {
    sessions_started_.push_back(std::make_pair(scan_session, error_code));
  }
  void OnDeviceFound(device::BluetoothLowEnergyScanSession* scan_session,
                     device::BluetoothDevice* device) override {
    devices_found_.push_back(std::make_pair(scan_session, device));
  }
  void OnDeviceLost(device::BluetoothLowEnergyScanSession* scan_session,
                    device::BluetoothDevice* device) override {
    devices_lost_.push_back(std::make_pair(scan_session, device));
  }
  void OnSessionInvalidated(
      device::BluetoothLowEnergyScanSession* scan_session) override {
    sessions_invalidated_.push_back(scan_session);
  }

  const std::vector<std::pair<
      device::BluetoothLowEnergyScanSession*,
      std::optional<device::BluetoothLowEnergyScanSession::ErrorCode>>>&
  sessions_started() const {
    return sessions_started_;
  }

  const std::vector<std::pair<device::BluetoothLowEnergyScanSession*,
                              device::BluetoothDevice*>>&
  devices_found() const {
    return devices_found_;
  }

  const std::vector<std::pair<device::BluetoothLowEnergyScanSession*,
                              device::BluetoothDevice*>>&
  devices_lost() const {
    return devices_lost_;
  }

  const std::vector<
      raw_ptr<device::BluetoothLowEnergyScanSession, VectorExperimental>>&
  sessions_invalidated() const {
    return sessions_invalidated_;
  }

  base::WeakPtr<FakeBluetoothLowEnergyScanSessionDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::vector<std::pair<
      device::BluetoothLowEnergyScanSession*,
      std::optional<device::BluetoothLowEnergyScanSession::ErrorCode>>>
      sessions_started_;
  std::vector<std::pair<device::BluetoothLowEnergyScanSession*,
                        device::BluetoothDevice*>>
      devices_found_;
  std::vector<std::pair<device::BluetoothLowEnergyScanSession*,
                        device::BluetoothDevice*>>
      devices_lost_;
  std::vector<
      raw_ptr<device::BluetoothLowEnergyScanSession, VectorExperimental>>
      sessions_invalidated_;

  base::WeakPtrFactory<FakeBluetoothLowEnergyScanSessionDelegate>
      weak_ptr_factory_{this};
};
#endif  // BUILDFLAG(IS_CHROMEOS)
}  // namespace

class BluetoothBlueZTest : public testing::Test {
 public:
  BluetoothBlueZTest() = default;

  static const char kGapUuid[];
  static const char kGattUuid[];
  static const char kPnpUuid[];
  static const char kHeadsetUuid[];

  void SetUp() override {
    std::unique_ptr<bluez::BluezDBusManagerSetter> dbus_setter =
        bluez::BluezDBusManager::GetSetterForTesting();

    auto fake_bluetooth_adapter_client =
        std::make_unique<bluez::FakeBluetoothAdapterClient>();
    fake_bluetooth_adapter_client->SetSimulationIntervalMs(10);

    auto fake_bluetooth_admin_policy_client =
        std::make_unique<bluez::FakeBluetoothAdminPolicyClient>();

    auto fake_bluetooth_battery_client =
        std::make_unique<bluez::FakeBluetoothBatteryClient>();

    auto fake_bluetooth_device_client =
        std::make_unique<bluez::FakeBluetoothDeviceClient>();
    // Use the original fake behavior for these tests.
    fake_bluetooth_device_client->set_delay_start_discovery(true);

    fake_bluetooth_adapter_client_ = fake_bluetooth_adapter_client.get();
    fake_bluetooth_admin_policy_client_ =
        fake_bluetooth_admin_policy_client.get();
    fake_bluetooth_battery_client_ = fake_bluetooth_battery_client.get();
    fake_bluetooth_device_client_ = fake_bluetooth_device_client.get();

    // We need to initialize BluezDBusManager early to prevent
    // Bluetooth*::Create() methods from picking the real instead of fake
    // implementations.
    dbus_setter->SetBluetoothAdapterClient(
        std::move(fake_bluetooth_adapter_client));
    dbus_setter->SetBluetoothAdminPolicyClient(
        std::move(fake_bluetooth_admin_policy_client));
    dbus_setter->SetBluetoothBatteryClient(
        std::move(fake_bluetooth_battery_client));
    dbus_setter->SetBluetoothDeviceClient(
        std::move(fake_bluetooth_device_client));
    dbus_setter->SetBluetoothInputClient(
        std::make_unique<bluez::FakeBluetoothInputClient>());
    dbus_setter->SetBluetoothAgentManagerClient(
        std::make_unique<bluez::FakeBluetoothAgentManagerClient>());
    dbus_setter->SetBluetoothGattServiceClient(
        std::make_unique<bluez::FakeBluetoothGattServiceClient>());

#if BUILDFLAG(IS_CHROMEOS)
    device::BluetoothAdapterFactory::SetBleScanParserCallback(
        base::BindLambdaForTesting([&]() {
          mojo::PendingRemote<data_decoder::mojom::BleScanParser>
              ble_scan_parser;
          mojo::MakeSelfOwnedReceiver(
              std::make_unique<FakeBleScanParserImpl>(),
              ble_scan_parser.InitWithNewPipeAndPassReceiver());
          return ble_scan_parser;
        }));
#endif  // BUILDFLAG(IS_CHROMEOS)

    callback_count_ = 0;
    error_callback_count_ = 0;
    last_client_error_ = "";
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS)
    device::BluetoothAdapterFactory::SetBleScanParserCallback(
        base::NullCallback());
#endif  // BUILDFLAG(IS_CHROMEOS)

    discovery_sessions_.clear();
    adapter_.reset();
    bluez::BluezDBusManager::Shutdown();
  }

  void Callback(base::OnceClosure quit_closure) {
    ++callback_count_;
    std::move(quit_closure).Run();
  }

  void CallbackWithClosure(base::OnceClosure closure) {
    ++callback_count_;
    std::move(closure).Run();
  }

  base::OnceClosure GetCallback(base::OnceClosure quit_closure) {
    return base::BindOnce(&BluetoothBlueZTest::Callback, base::Unretained(this),
                          std::move(quit_closure));
  }

  void DiscoverySessionCallback(
      std::unique_ptr<BluetoothDiscoverySession> discovery_session) {
    ++callback_count_;
    discovery_sessions_.push_back(std::move(discovery_session));
  }

  void ProfileRegisteredCallback(BluetoothAdapterProfileBlueZ* profile) {
    adapter_profile_ = profile;
    ++callback_count_;
  }

  void ErrorCallback(base::OnceClosure quit_closure) {
    ++error_callback_count_;
    std::move(quit_closure).Run();
  }

  base::OnceClosure GetErrorCallback(base::OnceClosure quit_closure) {
    return base::BindOnce(&BluetoothBlueZTest::ErrorCallback,
                          base::Unretained(this), std::move(quit_closure));
  }

  void DBusErrorCallback(const std::string& error_name,
                         const std::string& error_message) {
    ++error_callback_count_;
    last_client_error_ = error_name;
  }

  void ErrorCompletionCallback(const std::string& error_message) {
    ++error_callback_count_;
  }
  bool SetPoweredBlocking(bool powered) {
    base::test::TestFuture<bool> future;
    adapter_->SetPowered(powered, base::BindOnce(future.GetCallback(), true),
                         base::BindOnce(future.GetCallback(), false));
    return future.Get();
  }

  std::unique_ptr<BluetoothDiscoverySession> StartDiscoverySessionBlocking() {
    return StartDiscoverySessionWithFilterBlocking(nullptr);
  }

  std::unique_ptr<BluetoothDiscoverySession>
  StartDiscoverySessionWithFilterBlocking(
      std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter) {
    base::test::TestFuture<std::unique_ptr<BluetoothDiscoverySession>> future;
    StrictMock<base::MockOnceClosure> error_callback;
    adapter_->StartDiscoverySessionWithFilter(std::move(discovery_filter),
                                              /*client_name=*/std::string(),
                                              future.GetCallback(),
                                              error_callback.Get());
    return future.Take();
  }

  int NumActiveDiscoverySessions() {
    int count = 0;
    for (const auto& session : discovery_sessions_) {
      if (session->IsActive())
        count++;
    }
    return count;
  }

  bool IsAdapterDiscovering() {
    BluetoothAdapterBlueZ* adapter_bluez =
        static_cast<BluetoothAdapterBlueZ*>(adapter_.get());
    return adapter_bluez->IsDiscoveringForTesting();
  }

  // Call to fill the adapter_ member with a BluetoothAdapter instance.
  void GetAdapter() {
    adapter_ = BluetoothAdapterBlueZ::CreateAdapter();
    base::RunLoop run_loop;
    adapter_->Initialize(run_loop.QuitClosure());
    run_loop.Run();
    ASSERT_TRUE(adapter_);
    ASSERT_TRUE(adapter_->IsInitialized());
  }

  // Run a discovery phase until the named device is detected, or if the named
  // device is not created, the discovery process ends without finding it.
  //
  // The correct behavior of discovery is tested by the "Discovery" test case
  // without using this function.
  void DiscoverDevice(const std::string& address) {
    ASSERT_TRUE(adapter_.get() != nullptr);
    ASSERT_TRUE(base::CurrentThread::IsSet());
    fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

    TestBluetoothAdapterObserver observer(adapter_);
    base::test::RepeatingTestFuture<void> discovering_changed;
    observer.RegisterDiscoveringChangedWatcher(
        discovering_changed.GetCallback());

    EXPECT_TRUE(SetPoweredBlocking(true));
    ASSERT_TRUE(adapter_->IsPowered());

    auto discovery_session = StartDiscoverySessionBlocking();
    ASSERT_TRUE(discovery_session->IsActive());

    // The change to the discovering state is asynchronous and may happen after
    // the discovery session is started.
    discovering_changed.Take();
    EXPECT_EQ(1, observer.discovering_changed_count());
    EXPECT_TRUE(observer.last_discovering());
    ASSERT_TRUE(IsAdapterDiscovering());

    while (!observer.device_removed_count() &&
           observer.last_device_address() != address) {
      base::RunLoop loop;
      observer.set_quit_closure(loop.QuitWhenIdleClosure());
      loop.Run();
    }

    discovery_session.reset();
  }

  // Run a discovery phase so we have devices that can be paired with.
  void DiscoverDevices() {
    // Pass an invalid address for the device so that the discovery process
    // completes with all devices.
    DiscoverDevice("does not exist");
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<bluez::FakeBluetoothAdapterClient, DanglingUntriaged>
      fake_bluetooth_adapter_client_;
  raw_ptr<bluez::FakeBluetoothAdminPolicyClient, DanglingUntriaged>
      fake_bluetooth_admin_policy_client_;
  raw_ptr<bluez::FakeBluetoothBatteryClient, DanglingUntriaged>
      fake_bluetooth_battery_client_;
  raw_ptr<bluez::FakeBluetoothDeviceClient, DanglingUntriaged>
      fake_bluetooth_device_client_;
  scoped_refptr<BluetoothAdapter> adapter_;

  int callback_count_;
  int error_callback_count_;
  std::string last_client_error_;
  std::vector<std::unique_ptr<BluetoothDiscoverySession>> discovery_sessions_;
  raw_ptr<BluetoothAdapterProfileBlueZ> adapter_profile_;
#if BUILDFLAG(IS_CHROMEOS)
  base::HistogramTester histogram_tester_;
#endif
};

// This class was created to test BluetoothDeviceBluez::Connect() and
// BluetoothDeviceBluez::ConnectClassic(), two nearly identical methods, without
// having to repeat each test. Each test that would call either of these methods
// invokes PerformConnect() which will choose either
// BluetoothDeviceBluez::Connect() or BluetoothDeviceBluez::ConnectClassic()
// depending on the boolean returned by GetParam().
class BluetoothBlueZTestP : public BluetoothBlueZTest,
                            public testing::WithParamInterface<bool> {
 protected:
  // Invokes BluetoothDeviceBluez::ConnectClassic() on |device| when GetParam()
  // returns |true|, otherwise invokes BluetoothDeviceBluez::Connect().
  void PerformConnect(
      BluetoothDevice* device,
      device::BluetoothDevice::PairingDelegate* pairing_delegate,
      device::BluetoothDevice::ConnectCallback callback) {
#if BUILDFLAG(IS_CHROMEOS)
    if (GetParam()) {
      device->ConnectClassic(pairing_delegate, std::move(callback));
      return;
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
    device->Connect(pairing_delegate, std::move(callback));
  }
};

#if BUILDFLAG(IS_CHROMEOS)
INSTANTIATE_TEST_SUITE_P(All,
                         BluetoothBlueZTestP,
                         /*should_use_connect_classic=*/testing::Bool());
#else   // BUILDFLAG(IS_CHROMEOS)
INSTANTIATE_TEST_SUITE_P(All,
                         BluetoothBlueZTestP,
                         /*should_use_connect_classic=*/testing::Values(false));
#endif  // BUILDFLAG(IS_CHROMEOS)

const char BluetoothBlueZTest::kGapUuid[] =
    "00001800-0000-1000-8000-00805f9b34fb";
const char BluetoothBlueZTest::kGattUuid[] =
    "00001801-0000-1000-8000-00805f9b34fb";
const char BluetoothBlueZTest::kPnpUuid[] =
    "00001200-0000-1000-8000-00805f9b34fb";
const char BluetoothBlueZTest::kHeadsetUuid[] =
    "00001112-0000-1000-8000-00805f9b34fb";

TEST_F(BluetoothBlueZTest, AlreadyPresent) {
  GetAdapter();

  // This verifies that the class gets the list of adapters when created;
  // and initializes with an existing adapter if there is one.
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_EQ(bluez::FakeBluetoothAdapterClient::kAdapterAddress,
            adapter_->GetAddress());
  EXPECT_FALSE(IsAdapterDiscovering());

  // There should be 2 devices
  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  EXPECT_EQ(2U, devices.size());

  // |devices| are not ordered, verify it contains the 2 device addresses.
  EXPECT_NE(
      -1, GetDeviceIndexByAddress(
              devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress));
  EXPECT_NE(
      -1,
      GetDeviceIndexByAddress(
          devices,
          bluez::FakeBluetoothDeviceClient::kPairedUnconnectableDeviceAddress));
}

TEST_F(BluetoothBlueZTest, BecomePresent) {
  fake_bluetooth_adapter_client_->SetPresent(false);
  GetAdapter();
  ASSERT_FALSE(adapter_->IsPresent());

  // Install an observer; expect the AdapterPresentChanged to be called
  // with true, and IsPresent() to return true.
  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetPresent(true);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_TRUE(observer.last_present());

  EXPECT_TRUE(adapter_->IsPresent());

  // We should have had a device announced.
  EXPECT_EQ(2, observer.device_added_count());
  EXPECT_EQ(bluez::FakeBluetoothDeviceClient::kPairedUnconnectableDeviceAddress,
            observer.last_device_address());

  // Other callbacks shouldn't be called if the values are false.
  EXPECT_EQ(0, observer.powered_changed_count());
  EXPECT_EQ(0, observer.discovering_changed_count());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothBlueZTest, BecomeNotPresent) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  // Install an observer; expect the AdapterPresentChanged to be called
  // with false, and IsPresent() to return false.
  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetPresent(false);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_FALSE(observer.last_present());

  EXPECT_FALSE(adapter_->IsPresent());

  // We should have had 2 devices removed.
  EXPECT_EQ(2, observer.device_removed_count());
  // 2 possibilities for the last device here.
  std::string address = observer.last_device_address();
  EXPECT_TRUE(address.compare(bluez::FakeBluetoothDeviceClient::
                                  kPairedUnconnectableDeviceAddress) == 0 ||
              address.compare(
                  bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress) == 0);

  // DiscoveringChanged() should be triggered regardless of the current value
  // of Discovering property.
  EXPECT_EQ(1, observer.discovering_changed_count());

  // Other callbacks shouldn't be called since the values are false.
  EXPECT_EQ(0, observer.powered_changed_count());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothBlueZTest, SecondAdapter) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  // Install an observer, then add a second adapter. Nothing should change,
  // we ignore the second adapter.
  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetSecondPresent(true);

  EXPECT_EQ(0, observer.present_changed_count());

  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_EQ(bluez::FakeBluetoothAdapterClient::kAdapterAddress,
            adapter_->GetAddress());

  // Try removing the first adapter, we should now act as if the adapter
  // is no longer present rather than fall back to the second.
  fake_bluetooth_adapter_client_->SetPresent(false);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_FALSE(observer.last_present());

  EXPECT_FALSE(adapter_->IsPresent());

  // We should have had 2 devices removed.
  EXPECT_EQ(2, observer.device_removed_count());
  // As BluetoothAdapter devices removal does not keep the order of adding them,
  // 2 possibilities for the last device here.
  std::string address = observer.last_device_address();
  EXPECT_TRUE(address.compare(bluez::FakeBluetoothDeviceClient::
                                  kPairedUnconnectableDeviceAddress) == 0 ||
              address.compare(
                  bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress) == 0);

  // DiscoveringChanged() should be triggered regardless of the current value
  // of Discovering property.
  EXPECT_EQ(1, observer.discovering_changed_count());

  // Other callbacks shouldn't be called since the values are false.
  EXPECT_EQ(0, observer.powered_changed_count());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(IsAdapterDiscovering());

  observer.Reset();

  // Removing the second adapter shouldn't set anything either.
  fake_bluetooth_adapter_client_->SetSecondPresent(false);

  EXPECT_EQ(0, observer.device_removed_count());
  EXPECT_EQ(0, observer.powered_changed_count());
  EXPECT_EQ(0, observer.discovering_changed_count());
}

TEST_F(BluetoothBlueZTest, BecomePowered) {
  GetAdapter();
  ASSERT_FALSE(adapter_->IsPowered());

  // Install an observer; expect the AdapterPoweredChanged to be called
  // with true, and IsPowered() to return true.
  TestBluetoothAdapterObserver observer(adapter_);

  EXPECT_TRUE(SetPoweredBlocking(true));

  EXPECT_EQ(1, observer.powered_changed_count());
  EXPECT_TRUE(observer.last_powered());

  EXPECT_TRUE(adapter_->IsPowered());
}

TEST_F(BluetoothBlueZTest, BecomeNotPowered) {
  GetAdapter();
  EXPECT_TRUE(SetPoweredBlocking(true));

  ASSERT_TRUE(adapter_->IsPowered());

  // Install an observer; expect the AdapterPoweredChanged to be called
  // with false, and IsPowered() to return false.
  TestBluetoothAdapterObserver observer(adapter_);

  EXPECT_TRUE(SetPoweredBlocking(false));

  EXPECT_EQ(1, observer.powered_changed_count());
  EXPECT_FALSE(observer.last_powered());

  EXPECT_FALSE(adapter_->IsPowered());
}

TEST_F(BluetoothBlueZTest, SetPoweredWhenNotPresent) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  // Install an observer; expect the AdapterPresentChanged to be called
  // with false, and IsPresent() to return false.
  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetPresent(false);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_FALSE(observer.last_present());

  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());

  EXPECT_FALSE(SetPoweredBlocking(true));

  EXPECT_EQ(0, observer.powered_changed_count());
  EXPECT_FALSE(observer.last_powered());

  EXPECT_FALSE(adapter_->IsPowered());
}

TEST_F(BluetoothBlueZTest, ChangeAdapterName) {
  GetAdapter();

  const std::string new_name(".__.");
  base::RunLoop loop;
  adapter_->SetName(new_name, GetCallback(loop.QuitClosure()),
                    GetErrorCallback(loop.QuitClosure()));
  loop.Run();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(new_name, adapter_->GetName());
}

TEST_F(BluetoothBlueZTest, ChangeAdapterNameWhenNotPresent) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  // Install an observer; expect the AdapterPresentChanged to be called
  // with false, and IsPresent() to return false.
  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetPresent(false);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_FALSE(observer.last_present());

  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());
  base::RunLoop loop;
  adapter_->SetName("^o^", GetCallback(loop.QuitClosure()),
                    GetErrorCallback(loop.QuitClosure()));
  loop.Run();
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ("", adapter_->GetName());
}

TEST_F(BluetoothBlueZTest, GetUUIDs) {
  std::vector<std::string> adapterUuids;
  GetAdapter();

  adapterUuids.push_back(kGapUuid);
  adapterUuids.push_back(kGattUuid);
  adapterUuids.push_back(kPnpUuid);
  adapterUuids.push_back(kHeadsetUuid);

  fake_bluetooth_adapter_client_->SetUUIDs(adapterUuids);

  BluetoothAdapter::UUIDList uuids = adapter_->GetUUIDs();

  ASSERT_EQ(4U, uuids.size());
  // Check that the UUIDs match those from above - in order, GAP, GATT, PnP, and
  // headset.
  EXPECT_EQ(uuids[0], BluetoothUUID("1800"));
  EXPECT_EQ(uuids[1], BluetoothUUID("1801"));
  EXPECT_EQ(uuids[2], BluetoothUUID("1200"));
  EXPECT_EQ(uuids[3], BluetoothUUID("1112"));
}

TEST_F(BluetoothBlueZTest, BecomeDiscoverable) {
  GetAdapter();
  ASSERT_FALSE(adapter_->IsDiscoverable());

  // Install an observer; expect the AdapterDiscoverableChanged to be called
  // with true, and IsDiscoverable() to return true.
  TestBluetoothAdapterObserver observer(adapter_);
  base::RunLoop loop;
  adapter_->SetDiscoverable(true, GetCallback(loop.QuitClosure()),
                            GetErrorCallback(loop.QuitClosure()));
  loop.Run();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.discoverable_changed_count());

  EXPECT_TRUE(adapter_->IsDiscoverable());
}

TEST_F(BluetoothBlueZTest, BecomeNotDiscoverable) {
  GetAdapter();
  base::RunLoop loop1;
  base::RunLoop loop2;
  adapter_->SetDiscoverable(true, GetCallback(loop1.QuitClosure()),
                            GetErrorCallback(loop1.QuitClosure()));
  loop1.Run();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsDiscoverable());

  // Install an observer; expect the AdapterDiscoverableChanged to be called
  // with false, and IsDiscoverable() to return false.
  TestBluetoothAdapterObserver observer(adapter_);

  adapter_->SetDiscoverable(false, GetCallback(loop2.QuitClosure()),
                            GetErrorCallback(loop2.QuitClosure()));
  loop2.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.discoverable_changed_count());

  EXPECT_FALSE(adapter_->IsDiscoverable());
}

TEST_F(BluetoothBlueZTest, SetDiscoverableWhenNotPresent) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());
  ASSERT_FALSE(adapter_->IsDiscoverable());

  // Install an observer; expect the AdapterDiscoverableChanged to be called
  // with true, and IsDiscoverable() to return true.
  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetPresent(false);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_FALSE(observer.last_present());

  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsDiscoverable());

  base::RunLoop loop;
  adapter_->SetDiscoverable(true, GetCallback(loop.QuitClosure()),
                            GetErrorCallback(loop.QuitClosure()));
  loop.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(0, observer.discoverable_changed_count());

  EXPECT_FALSE(adapter_->IsDiscoverable());
}

TEST_F(BluetoothBlueZTest, StopDiscovery) {
  GetAdapter();
  EXPECT_TRUE(SetPoweredBlocking(true));
  ASSERT_TRUE(adapter_->IsPowered());

  TestBluetoothAdapterObserver observer(adapter_);
  base::test::RepeatingTestFuture<void> discovering_changed;
  observer.RegisterDiscoveringChangedWatcher(discovering_changed.GetCallback());

  auto discovery_session = StartDiscoverySessionBlocking();
  ASSERT_TRUE(discovery_session->IsActive());

  // The change to the discovering state is asynchronous and may happen after
  // the discovery session is started.
  discovering_changed.Take();
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  ASSERT_TRUE(IsAdapterDiscovering());

  discovery_session.reset();

  discovering_changed.Take();
  EXPECT_EQ(2, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());
}

TEST_F(BluetoothBlueZTest, Discovery) {
  // Test a simulated discovery session.
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);
  GetAdapter();
  EXPECT_TRUE(SetPoweredBlocking(true));
  ASSERT_TRUE(adapter_->IsPowered());

  TestBluetoothAdapterObserver observer(adapter_);
  base::test::RepeatingTestFuture<void> discovering_changed;
  observer.RegisterDiscoveringChangedWatcher(discovering_changed.GetCallback());

  auto discovery_session = StartDiscoverySessionBlocking();
  ASSERT_TRUE(discovery_session->IsActive());

  discovering_changed.Take();
  ASSERT_TRUE(IsAdapterDiscovering());

  // First two devices to appear.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }
  EXPECT_EQ(2, observer.device_added_count());
  EXPECT_EQ(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress,
            observer.last_device_address());

  // Next we should get another two devices...
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }
  EXPECT_EQ(4, observer.device_added_count());

  // Okay, let's run forward until a device is actually removed...
  while (!observer.device_removed_count()) {
    {
      base::RunLoop loop;
      observer.set_quit_closure(loop.QuitWhenIdleClosure());
      loop.Run();
    }
  }

  EXPECT_EQ(1, observer.device_removed_count());
  EXPECT_EQ(bluez::FakeBluetoothDeviceClient::kVanishingDeviceAddress,
            observer.last_device_address());
}

TEST_F(BluetoothBlueZTest, PoweredAndDiscovering) {
  GetAdapter();
  EXPECT_TRUE(SetPoweredBlocking(true));

  TestBluetoothAdapterObserver observer(adapter_);
  base::test::RepeatingTestFuture<void> discovering_changed;
  observer.RegisterDiscoveringChangedWatcher(discovering_changed.GetCallback());

  auto discovery_session = StartDiscoverySessionBlocking();
  ASSERT_TRUE(discovery_session->IsActive());

  // Stop the timers that the simulation uses
  fake_bluetooth_device_client_->EndDiscoverySimulation(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath));

  discovering_changed.Take();
  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(IsAdapterDiscovering());

  fake_bluetooth_adapter_client_->SetPresent(false);
  ASSERT_FALSE(adapter_->IsPresent());
  ASSERT_FALSE(discovery_session->IsActive());

  observer.Reset();

  // Expect the AdapterPresentChanged, AdapterPoweredChanged and
  // AdapterDiscoveringChanged methods to be called with true, and IsPresent(),
  // IsPowered() and IsDiscoveringForTesting() to all return true.
  fake_bluetooth_adapter_client_->SetPresent(true);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_TRUE(observer.last_present());
  EXPECT_TRUE(adapter_->IsPresent());

  EXPECT_EQ(1, observer.powered_changed_count());
  EXPECT_TRUE(observer.last_powered());
  EXPECT_TRUE(adapter_->IsPowered());

  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());

  observer.Reset();

  // Now mark the adapter not present again. Expect the methods to be called
  // again, to reset the properties back to false.
  fake_bluetooth_adapter_client_->SetPresent(false);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_FALSE(observer.last_present());
  EXPECT_FALSE(adapter_->IsPresent());

  EXPECT_EQ(1, observer.powered_changed_count());
  EXPECT_FALSE(observer.last_powered());
  EXPECT_FALSE(adapter_->IsPowered());

  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());
}

// This unit test asserts that a DiscoverySession which is queued to start does
// not get interrupted by a stop discovery call being executed.
TEST_F(BluetoothBlueZTest, StopAndStartDiscoverySimultaneously) {
  GetAdapter();
  EXPECT_TRUE(SetPoweredBlocking(true));
  EXPECT_TRUE(adapter_->IsPowered());

  TestBluetoothAdapterObserver observer(adapter_);

  EXPECT_EQ(0, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());

  // Start Discovery in order to call Stop.
  base::test::RepeatingTestFuture<void> discovering_changed;
  observer.RegisterDiscoveringChangedWatcher(discovering_changed.GetCallback());
  auto discovery_session = StartDiscoverySessionBlocking();

  // Validate states when 1st StartDiscovery call finished and observer is
  // notified.
  discovering_changed.Take();
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());

  // Reset the only active session to initiate a StopDiscovery request.
  discovery_session.reset();

  // At this moment it only observe discovery state change once since
  // StopDiscovery is ongoing.
  EXPECT_EQ(1, observer.discovering_changed_count());

  // Queue up StartDiscovery request and ensure ongoing StopDiscovery request
  // isn't interrupted.
  base::test::RepeatingTestFuture<std::unique_ptr<BluetoothDiscoverySession>>
      future;
  StrictMock<base::MockOnceClosure> error_callback;
  adapter_->StartDiscoverySession(
      /*client_name=*/std::string(), future.GetCallback(),
      error_callback.Get());

  // Ensure the ongoing StopDiscovery request finished and observer is notified
  // before following StartDiscovery request started.
  discovering_changed.Take();
  EXPECT_EQ(2, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());

  // Ensure last StartDiscovery call finished.
  discovery_session = future.Take();
  discovering_changed.Take();
  EXPECT_EQ(3, observer.discovering_changed_count());
  EXPECT_TRUE(IsAdapterDiscovering());
  EXPECT_TRUE(discovery_session->IsActive());
}

// This unit test asserts that the basic reference counting logic works
// correctly for discovery requests done via the BluetoothAdapter.
TEST_F(BluetoothBlueZTest, MultipleDiscoverySessions) {
  GetAdapter();
  EXPECT_TRUE(SetPoweredBlocking(true));
  EXPECT_TRUE(adapter_->IsPowered());

  TestBluetoothAdapterObserver observer(adapter_);
  base::test::RepeatingTestFuture<void> discovering_changed;
  observer.RegisterDiscoveringChangedWatcher(discovering_changed.GetCallback());

  EXPECT_EQ(0, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());

  // Request device discovery 3 times.
  base::test::RepeatingTestFuture<std::unique_ptr<BluetoothDiscoverySession>>
      future;
  StrictMock<base::MockOnceClosure> error_callback;
  for (int i = 0; i < 3; i++) {
    adapter_->StartDiscoverySession(
        /*client_name=*/std::string(), future.GetCallback(),
        error_callback.Get());
  }

  for (int i = 0; i < 3; i++) {
    discovery_sessions_.push_back(future.Take());
  }

  // The observer should have received the discovering changed event exactly
  // once, the success callback should have been called 3 times and the adapter
  // should be discovering.
  discovering_changed.Take();
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());

  // Request to stop discovery twice.
  for (int i = 0; i < 2; i++) {
    discovery_sessions_.erase(discovery_sessions_.begin());
  }

  // The observer should have received no additional discovering changed events,
  // and the adapter should still be discovering.
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  // Request device discovery 3 times.
  for (int i = 0; i < 3; i++) {
    adapter_->StartDiscoverySession(
        /*client_name=*/std::string(), future.GetCallback(),
        error_callback.Get());
  }

  for (int i = 0; i < 3; i++) {
    discovery_sessions_.push_back(future.Take());
  }

  // The observer should have received no additional discovering changed events,
  // the adapter should still be discovering.
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ(4u, discovery_sessions_.size());

  // Request to stop discovery 4 times.
  for (int i = 0; i < 4; i++) {
    discovery_sessions_.erase(discovery_sessions_.begin());
  }
  EXPECT_TRUE(discovery_sessions_.empty());

  // The observer should have received the discovering changed event exactly
  // once, the adapter should no longer be discovering.
  discovering_changed.Take();
  EXPECT_EQ(2, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());
}

// This unit test asserts that the reference counting logic works correctly in
// the cases when the adapter gets reset and D-Bus calls are made outside of
// the BluetoothAdapter.
TEST_F(BluetoothBlueZTest, UnexpectedChangesDuringMultipleDiscoverySessions) {
  GetAdapter();
  EXPECT_TRUE(SetPoweredBlocking(true));
  EXPECT_TRUE(adapter_->IsPowered());

  TestBluetoothAdapterObserver observer(adapter_);
  base::test::RepeatingTestFuture<void> discovering_changed;
  observer.RegisterDiscoveringChangedWatcher(discovering_changed.GetCallback());

  EXPECT_EQ(0, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());

  base::test::RepeatingTestFuture<std::unique_ptr<BluetoothDiscoverySession>>
      start_discovery_future;
  StrictMock<base::MockOnceClosure> error_callback;

  // Request device discovery 3 times.
  for (int i = 0; i < 3; i++) {
    adapter_->StartDiscoverySession(
        /*client_name=*/std::string(), start_discovery_future.GetCallback(),
        error_callback.Get());
  }

  for (int i = 0; i < 3; i++) {
    discovery_sessions_.push_back(start_discovery_future.Take());
  }

  // Wait till observer was notified for discovery state changed before checking
  // states.
  discovering_changed.Take();
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());

  for (int i = 0; i < 3; i++)
    EXPECT_TRUE(discovery_sessions_[i]->IsActive());

  // Stop the timers that the simulation uses
  fake_bluetooth_device_client_->EndDiscoverySimulation(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath));

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(IsAdapterDiscovering());

  // Stop device discovery behind the adapter. The adapter and the observer
  // should be notified of the change and the reference count should be reset.
  // Even though bluez::FakeBluetoothAdapterClient does its own reference
  // counting and
  // we called 3 BluetoothAdapter::StartDiscoverySession 3 times, the
  // bluez::FakeBluetoothAdapterClient's count should be only 1 and a single
  // call to
  // bluez::FakeBluetoothAdapterClient::StopDiscovery should work.
  base::test::RepeatingTestFuture<void> stop_discovery_future;
  StrictMock<MockDBusErrorCallback> dbus_error_callback;
  fake_bluetooth_adapter_client_->BluetoothAdapterClient::StopDiscovery(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      stop_discovery_future.GetCallback(), dbus_error_callback.Get());
  stop_discovery_future.Take();

  discovering_changed.Take();
  EXPECT_EQ(2, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());

  // All discovery session instances should have been updated.
  for (int i = 0; i < 3; i++)
    EXPECT_FALSE(discovery_sessions_[i]->IsActive());
  discovery_sessions_.clear();

  // It should be possible to successfully start discovery.
  for (int i = 0; i < 2; i++) {
    adapter_->StartDiscoverySession(
        /*client_name=*/std::string(), start_discovery_future.GetCallback(),
        error_callback.Get());
  }

  for (int i = 0; i < 2; i++) {
    discovery_sessions_.push_back(start_discovery_future.Take());
  }

  discovering_changed.Take();
  EXPECT_EQ(3, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)2, discovery_sessions_.size());

  for (int i = 0; i < 2; i++)
    EXPECT_TRUE(discovery_sessions_[i]->IsActive());

  fake_bluetooth_device_client_->EndDiscoverySimulation(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath));

  // Make the adapter disappear and appear. This will make it come back as
  // discovering. When this happens, the reference count should become and
  // remain 0 as no new request was made through the BluetoothAdapter.
  fake_bluetooth_adapter_client_->SetPresent(false);

  discovering_changed.Take();
  ASSERT_FALSE(adapter_->IsPresent());
  EXPECT_EQ(4, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());

  for (int i = 0; i < 2; i++)
    EXPECT_FALSE(discovery_sessions_[i]->IsActive());
  discovery_sessions_.clear();

  fake_bluetooth_adapter_client_->SetPresent(true);

  discovering_changed.Take();
  ASSERT_TRUE(adapter_->IsPresent());
  EXPECT_EQ(5, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());

  // Start and stop discovery. At this point, bluez::FakeBluetoothAdapterClient
  // has
  // a reference count that is equal to 1. Pretend that this was done by an
  // application other than us. Starting and stopping discovery will succeed
  // but it won't cause the discovery state to change.
  discovery_sessions_.push_back(StartDiscoverySessionBlocking());

  EXPECT_EQ(5, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  discovery_sessions_.clear();
  EXPECT_EQ(5, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  EXPECT_TRUE(discovery_sessions_.empty());

  // Start discovery again.
  discovery_sessions_.push_back(StartDiscoverySessionBlocking());

  EXPECT_EQ(5, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  // Stop discovery via D-Bus. The fake client's reference count will drop but
  // the discovery state won't change since our BluetoothAdapter also just
  // requested it via D-Bus.
  fake_bluetooth_adapter_client_->BluetoothAdapterClient::StopDiscovery(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      stop_discovery_future.GetCallback(), dbus_error_callback.Get());
  stop_discovery_future.Take();
  EXPECT_EQ(5, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());

  // Now end the discovery session. This should change the adapter's discovery
  // state.
  discovery_sessions_.clear();
  discovering_changed.Take();
  EXPECT_EQ(6, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());
  EXPECT_TRUE(discovery_sessions_.empty());
}

TEST_F(BluetoothBlueZTest, InvalidatedDiscoverySessions) {
  GetAdapter();
  EXPECT_TRUE(SetPoweredBlocking(true));
  EXPECT_TRUE(adapter_->IsPowered());

  TestBluetoothAdapterObserver observer(adapter_);
  base::test::RepeatingTestFuture<void> discovering_changed;
  observer.RegisterDiscoveringChangedWatcher(discovering_changed.GetCallback());

  EXPECT_EQ(0, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());

  base::test::RepeatingTestFuture<std::unique_ptr<BluetoothDiscoverySession>>
      future;
  StrictMock<base::MockOnceClosure> error_callback;
  // Request device discovery 3 times.
  for (int i = 0; i < 3; i++) {
    adapter_->StartDiscoverySession(
        /*client_name=*/std::string(), future.GetCallback(),
        error_callback.Get());
  }

  for (int i = 0; i < 3; i++) {
    discovery_sessions_.push_back(future.Take());
  }

  // Wait till observer was notified for discovery state changed before checking
  // states.
  discovering_changed.Take();
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());

  for (int i = 0; i < 3; i++)
    EXPECT_TRUE(discovery_sessions_[i]->IsActive());

  // Stop the timers that the simulation uses
  fake_bluetooth_device_client_->EndDiscoverySimulation(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath));

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(IsAdapterDiscovering());

  // Delete all but one discovery session.
  discovery_sessions_.pop_back();
  discovery_sessions_.pop_back();
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());
  EXPECT_TRUE(IsAdapterDiscovering());

  // Stop device discovery behind the adapter. The one active discovery session
  // should become inactive, but more importantly, we shouldn't run into any
  // memory errors as the sessions that we explicitly deleted should get
  // cleaned up.
  StrictMock<MockDBusErrorCallback> dbus_error_callback;
  base::test::RepeatingTestFuture<void> stop_discovery_future;
  fake_bluetooth_adapter_client_->BluetoothAdapterClient::StopDiscovery(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      stop_discovery_future.GetCallback(), dbus_error_callback.Get());
  stop_discovery_future.Take();
  discovering_changed.Take();
  EXPECT_EQ(2, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());
  EXPECT_FALSE(discovery_sessions_[0]->IsActive());
}

TEST_F(BluetoothBlueZTest, StartDiscoverySession) {
  GetAdapter();

  EXPECT_TRUE(SetPoweredBlocking(true));
  EXPECT_TRUE(adapter_->IsPowered());

  TestBluetoothAdapterObserver observer(adapter_);
  base::test::RepeatingTestFuture<void> discovering_changed;
  observer.RegisterDiscoveringChangedWatcher(discovering_changed.GetCallback());

  EXPECT_EQ(0, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());
  EXPECT_TRUE(discovery_sessions_.empty());

  // Request a new discovery session.
  discovery_sessions_.push_back(StartDiscoverySessionBlocking());
  discovering_changed.Take();
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  // Start another session. A new one should be returned in the callback, which
  // in turn will destroy the previous session. Adapter should still be
  // discovering and the reference count should be 1.
  discovery_sessions_.push_back(StartDiscoverySessionBlocking());
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)2, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  // Request a new session.
  discovery_sessions_.push_back(StartDiscoverySessionBlocking());
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ(3u, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[1]->IsActive());
  EXPECT_NE(discovery_sessions_[0], discovery_sessions_[1]);

  // Stop the previous discovery session. The session should end but discovery
  // should continue.
  discovery_sessions_.erase(discovery_sessions_.begin());
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ(2u, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  // Delete the current active session.
  discovery_sessions_.clear();
  discovering_changed.Take();
  EXPECT_EQ(2, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());
}

TEST_F(BluetoothBlueZTest, SetDiscoveryFilterBeforeStartDiscovery) {
  // Test a simulated discovery session.
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);
  GetAdapter();

  TestBluetoothAdapterObserver observer(adapter_);
  base::test::RepeatingTestFuture<void> discovering_changed;
  observer.RegisterDiscoveringChangedWatcher(discovering_changed.GetCallback());

  auto discovery_filter = std::make_unique<BluetoothDiscoveryFilter>(
      device::BLUETOOTH_TRANSPORT_LE);
  discovery_filter->SetRSSI(-60);
  device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
  device_filter.uuids.insert(BluetoothUUID("1000"));
  discovery_filter->AddDeviceFilter(std::move(device_filter));

  EXPECT_TRUE(SetPoweredBlocking(true));
  EXPECT_TRUE(adapter_->IsPowered());

  auto* comparison_filter_holder = discovery_filter.get();
  auto discovery_session =
      StartDiscoverySessionWithFilterBlocking(std::move(discovery_filter));

  discovering_changed.Take();
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  EXPECT_TRUE(discovery_session->IsActive());

  ASSERT_TRUE(comparison_filter_holder->Equals(
      *discovery_session->GetDiscoveryFilter()));

  auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_NE(nullptr, filter);
  EXPECT_EQ("le", *filter->transport);
  EXPECT_EQ(-60, *filter->rssi);
  EXPECT_EQ(nullptr, filter->pathloss.get());
  std::vector<std::string> uuids = *filter->uuids;
  EXPECT_TRUE(base::Contains(uuids, "1000"));

  discovery_session.reset();
  discovering_changed.Take();
  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_FALSE(IsAdapterDiscovering());

  filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ(nullptr, filter);
}

TEST_F(BluetoothBlueZTest, SetDiscoveryFilterBeforeStartDiscoveryFail) {
  // Test a simulated discovery session.
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);
  GetAdapter();

  TestBluetoothAdapterObserver observer(adapter_);

  auto discovery_filter = std::make_unique<BluetoothDiscoveryFilter>(
      device::BLUETOOTH_TRANSPORT_LE);
  discovery_filter->SetRSSI(-60);
  device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
  device_filter.uuids.insert(BluetoothUUID("1000"));
  discovery_filter->AddDeviceFilter(std::move(device_filter));

  EXPECT_TRUE(SetPoweredBlocking(true));

  fake_bluetooth_adapter_client_->MakeSetDiscoveryFilterFail();

  base::test::TestFuture<void> error_future;
  StrictMock<base::MockCallback<BluetoothAdapter::DiscoverySessionCallback>>
      success_callback;
  adapter_->StartDiscoverySessionWithFilter(std::move(discovery_filter),
                                            /*client_name=*/std::string(),
                                            success_callback.Get(),
                                            error_future.GetCallback());

  EXPECT_TRUE(error_future.Wait());
  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_FALSE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)0, discovery_sessions_.size());

  auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ(nullptr, filter);
}

// This unit test asserts that the basic reference counting, and filter merging
// works correctly for discovery requests done via the BluetoothAdapter.
TEST_F(BluetoothBlueZTest, SetDiscoveryFilterBeforeStartDiscoveryMultiple) {
  GetAdapter();
  EXPECT_TRUE(SetPoweredBlocking(true));
  EXPECT_TRUE(adapter_->IsPowered());

  TestBluetoothAdapterObserver observer(adapter_);
  base::test::RepeatingTestFuture<void> discoverying_changed;
  observer.RegisterDiscoveringChangedWatcher(
      discoverying_changed.GetCallback());

  // Request device discovery with pre-set filter 3 times.
  for (int i = 0; i < 3; i++) {
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter;
    if (i == 0) {
      discovery_filter = std::make_unique<BluetoothDiscoveryFilter>(
          device::BLUETOOTH_TRANSPORT_LE);
      discovery_filter->SetRSSI(-85);
      device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
      device_filter.uuids.insert(BluetoothUUID("1000"));
      discovery_filter->AddDeviceFilter(std::move(device_filter));
    } else if (i == 1) {
      discovery_filter = std::make_unique<BluetoothDiscoveryFilter>(
          device::BLUETOOTH_TRANSPORT_LE);
      discovery_filter->SetRSSI(-60);
      device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
      device_filter.uuids.insert(BluetoothUUID("1020"));
      device_filter.uuids.insert(BluetoothUUID("1001"));
      discovery_filter->AddDeviceFilter(std::move(device_filter));
    } else if (i == 2) {
      discovery_filter = std::make_unique<BluetoothDiscoveryFilter>(
          device::BLUETOOTH_TRANSPORT_LE);
      discovery_filter->SetRSSI(-65);
      device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
      device_filter.uuids.insert(BluetoothUUID("1020"));
      device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter2;
      device_filter2.uuids.insert(BluetoothUUID("1003"));
      discovery_filter->AddDeviceFilter(std::move(device_filter));
      discovery_filter->AddDeviceFilter(std::move(device_filter2));
    }

    discovery_sessions_.push_back(
        StartDiscoverySessionWithFilterBlocking(std::move(discovery_filter)));

    if (i == 0) {
      discoverying_changed.Take();
      EXPECT_EQ(1, observer.discovering_changed_count());
      observer.Reset();

      auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-85, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_TRUE(base::Contains(uuids, "1000"));
    } else if (i == 1) {
      auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-85, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_TRUE(base::Contains(uuids, "1000"));
      EXPECT_TRUE(base::Contains(uuids, "1001"));
      EXPECT_TRUE(base::Contains(uuids, "1020"));
    } else if (i == 2) {
      auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-85, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_TRUE(base::Contains(uuids, "1000"));
      EXPECT_TRUE(base::Contains(uuids, "1001"));
      EXPECT_TRUE(base::Contains(uuids, "1003"));
      EXPECT_TRUE(base::Contains(uuids, "1020"));
    }
  }

  // the success callback should have been called 3 times and the adapter should
  // be discovering.
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());

  // Request to stop discovery twice.
  // Note: Here it is using `RegisterDiscoveryChangeCompletedWatcher` which is
  // notified when `BluetoothAdapter::ProcessDiscoveryQueue`is called. That
  // means a session is created or destroyed. As for
  // `RegisterDiscoveringChangedWatcher`, it is used to observe when actual
  // discovery state of adapter changed. That means the first session is created
  // or the last session is destroyed.
  base::test::RepeatingTestFuture<void> discovery_session_changed;
  observer.RegisterDiscoveryChangeCompletedWatcher(
      discovery_session_changed.GetCallback());
  for (int i = 0; i < 2; i++) {
    discovery_sessions_.erase(discovery_sessions_.begin());
    discovery_session_changed.Take();
    if (i == 0) {
      auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-65, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_EQ(3UL, uuids.size());
      EXPECT_FALSE(base::Contains(uuids, "1000"));
      EXPECT_TRUE(base::Contains(uuids, "1001"));
      EXPECT_TRUE(base::Contains(uuids, "1003"));
      EXPECT_TRUE(base::Contains(uuids, "1020"));
    } else if (i == 1) {
      auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-65, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_EQ(2UL, uuids.size());
      EXPECT_FALSE(base::Contains(uuids, "1000"));
      EXPECT_FALSE(base::Contains(uuids, "1001"));
      EXPECT_TRUE(base::Contains(uuids, "1003"));
      EXPECT_TRUE(base::Contains(uuids, "1020"));
    }
  }

  // The success callback should have been called 2 times and the adapter should
  // still be discovering.
  EXPECT_TRUE(IsAdapterDiscovering());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());
  ASSERT_EQ(1u, discovery_sessions_.size());

  // Request device discovery 3 times.
  for (int i = 0; i < 3; i++) {
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter;

    if (i == 0) {
      discovery_filter = std::make_unique<BluetoothDiscoveryFilter>(
          device::BLUETOOTH_TRANSPORT_LE);
      discovery_filter->SetRSSI(-85);
      device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
      device_filter.uuids.insert(BluetoothUUID("1000"));
      discovery_filter->AddDeviceFilter(std::move(device_filter));
    } else if (i == 1) {
      discovery_filter = std::make_unique<BluetoothDiscoveryFilter>(
          device::BLUETOOTH_TRANSPORT_LE);
      discovery_filter->SetRSSI(-60);
      device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
      device_filter.uuids.insert(BluetoothUUID("1020"));
      device_filter.uuids.insert(BluetoothUUID("1001"));
      discovery_filter->AddDeviceFilter(std::move(device_filter));
    } else if (i == 2) {
      discovery_filter = std::make_unique<BluetoothDiscoveryFilter>(
          device::BLUETOOTH_TRANSPORT_LE);
      discovery_filter->SetRSSI(-65);
      device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
      device_filter.uuids.insert(BluetoothUUID("1020"));
      device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter2;
      device_filter2.uuids.insert(BluetoothUUID("1003"));
      discovery_filter->AddDeviceFilter(std::move(device_filter));
      discovery_filter->AddDeviceFilter(std::move(device_filter2));
    }

    discovery_sessions_.push_back(
        StartDiscoverySessionWithFilterBlocking(std::move(discovery_filter)));

    if (i == 0) {
      auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-85, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_TRUE(base::Contains(uuids, "1000"));
      EXPECT_TRUE(base::Contains(uuids, "1003"));
      EXPECT_TRUE(base::Contains(uuids, "1020"));
    } else if (i == 1 || i == 2) {
      auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-85, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_TRUE(base::Contains(uuids, "1000"));
      EXPECT_TRUE(base::Contains(uuids, "1001"));
      EXPECT_TRUE(base::Contains(uuids, "1003"));
      EXPECT_TRUE(base::Contains(uuids, "1020"));
    }
  }

  // The success callback should have been called 3 times and the adapter should
  // still be discovering.
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ(4u, discovery_sessions_.size());

  // Request to stop discovery 4 times.
  for (int i = 0; i < 4; i++) {
    discovery_sessions_.erase(discovery_sessions_.begin());
    discovery_session_changed.Take();
  }
  discoverying_changed.Take();

  // The adapter should no longer be discovering.
  EXPECT_FALSE(IsAdapterDiscovering());
  EXPECT_EQ(1, observer.discovering_changed_count());

  // All discovery sessions should be inactive.
  ASSERT_TRUE(discovery_sessions_.empty());
  auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ(nullptr, filter);
}

// This unit test asserts that filter merging logic works correctly for filtered
// discovery requests done via the BluetoothAdapter.
TEST_F(BluetoothBlueZTest, SetDiscoveryFilterMergingTest) {
  GetAdapter();
  EXPECT_TRUE(SetPoweredBlocking(true));

  std::unique_ptr<BluetoothDiscoveryFilter> df =
      std::make_unique<BluetoothDiscoveryFilter>(
          device::BLUETOOTH_TRANSPORT_LE);
  df->SetRSSI(-15);
  device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
  device_filter.uuids.insert(BluetoothUUID("1000"));
  df->AddDeviceFilter(std::move(device_filter));

  discovery_sessions_.push_back(
      StartDiscoverySessionWithFilterBlocking(std::move(df)));

  auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ("le", *filter->transport);
  EXPECT_EQ(-15, *filter->rssi);
  EXPECT_EQ(nullptr, filter->pathloss.get());
  std::vector<std::string> uuids = *filter->uuids;
  EXPECT_TRUE(base::Contains(uuids, "1000"));

  df = std::make_unique<BluetoothDiscoveryFilter>(
      device::BLUETOOTH_TRANSPORT_LE);
  df->SetRSSI(-60);
  device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter2;
  device_filter2.uuids.insert(BluetoothUUID("1020"));
  device_filter2.uuids.insert(BluetoothUUID("1001"));
  df->AddDeviceFilter(device_filter2);

  discovery_sessions_.push_back(
      StartDiscoverySessionWithFilterBlocking(std::move(df)));

  filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ("le", *filter->transport);
  EXPECT_EQ(-60, *filter->rssi);
  EXPECT_EQ(nullptr, filter->pathloss.get());
  uuids = *filter->uuids;
  EXPECT_TRUE(base::Contains(uuids, "1000"));
  EXPECT_TRUE(base::Contains(uuids, "1001"));
  EXPECT_TRUE(base::Contains(uuids, "1020"));

  BluetoothDiscoveryFilter* df3 =
      new BluetoothDiscoveryFilter(device::BLUETOOTH_TRANSPORT_CLASSIC);
  df3->SetRSSI(-65);
  device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter3;
  device_filter3.uuids.insert(BluetoothUUID("1020"));
  device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter4;
  device_filter4.uuids.insert(BluetoothUUID("1003"));
  df3->AddDeviceFilter(device_filter3);
  df3->AddDeviceFilter(device_filter4);
  std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter3(df3);

  discovery_sessions_.push_back(
      StartDiscoverySessionWithFilterBlocking(std::move(discovery_filter3)));

  filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ("auto", *filter->transport);
  EXPECT_EQ(-65, *filter->rssi);
  EXPECT_EQ(nullptr, filter->pathloss.get());
  uuids = *filter->uuids;
  EXPECT_TRUE(base::Contains(uuids, "1000"));
  EXPECT_TRUE(base::Contains(uuids, "1001"));
  EXPECT_TRUE(base::Contains(uuids, "1003"));
  EXPECT_TRUE(base::Contains(uuids, "1020"));

  // start additionally classic scan
  discovery_sessions_.push_back(StartDiscoverySessionBlocking());

  filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ("auto", *filter->transport);
  EXPECT_EQ(nullptr, filter->rssi.get());
  EXPECT_EQ(nullptr, filter->pathloss.get());
  EXPECT_EQ(nullptr, filter->uuids.get());
  discovery_sessions_.clear();
}

TEST_F(BluetoothBlueZTest, DeviceProperties) {
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);
  ASSERT_EQ(bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[idx]->GetAddress());

  // Verify the other device properties.
  EXPECT_EQ(
      base::UTF8ToUTF16(bluez::FakeBluetoothDeviceClient::kPairedDeviceName),
      devices[idx]->GetNameForDisplay());
  EXPECT_EQ(BluetoothDeviceType::COMPUTER, devices[idx]->GetDeviceType());
  EXPECT_TRUE(devices[idx]->IsPaired());
  EXPECT_FALSE(devices[idx]->IsConnected());
  EXPECT_FALSE(devices[idx]->IsConnecting());

  // Non HID devices are always connectable.
  EXPECT_TRUE(devices[idx]->IsConnectable());

  BluetoothDevice::UUIDSet uuids = devices[idx]->GetUUIDs();
  EXPECT_EQ(2U, uuids.size());
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("1800")));
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("1801")));

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_USB, devices[idx]->GetVendorIDSource());
  EXPECT_EQ(0x05ac, devices[idx]->GetVendorID());
  EXPECT_EQ(0x030d, devices[idx]->GetProductID());
  EXPECT_EQ(0x0306, devices[idx]->GetDeviceID());
}

TEST_F(BluetoothBlueZTest, DeviceAddressType) {
  GetAdapter();
  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);
  ASSERT_EQ(bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[idx]->GetAddress());

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  properties->address_type.set_valid(false);
  EXPECT_EQ(BluetoothDevice::ADDR_TYPE_UNKNOWN, devices[idx]->GetAddressType());

  properties->address_type.set_valid(true);

  properties->address_type.ReplaceValue(bluetooth_device::kAddressTypePublic);
  EXPECT_EQ(BluetoothDevice::ADDR_TYPE_PUBLIC, devices[idx]->GetAddressType());

  properties->address_type.ReplaceValue(bluetooth_device::kAddressTypeRandom);
  EXPECT_EQ(BluetoothDevice::ADDR_TYPE_RANDOM, devices[idx]->GetAddressType());
}

TEST_F(BluetoothBlueZTest, DeviceClassChanged) {
  // Simulate a change of class of a device, as sometimes occurs
  // during discovery.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);
  ASSERT_EQ(BluetoothDeviceType::COMPUTER, devices[idx]->GetDeviceType());

  // Install an observer; expect the DeviceChanged method to be called when
  // we change the class of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  properties->bluetooth_class.ReplaceValue(0x002580);

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());

  EXPECT_EQ(BluetoothDeviceType::MOUSE, devices[idx]->GetDeviceType());
}

TEST_F(BluetoothBlueZTest, DeviceAppearance) {
  // Simulate a device with appearance.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);
  ASSERT_EQ(BluetoothDeviceType::COMPUTER, devices[idx]->GetDeviceType());

  // Install an observer; expect the DeviceChanged method to be called when
  // we change the appearance of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  // Let the device come without bluetooth_class.
  properties->appearance.ReplaceValue(0);  // DeviceChanged method called
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());

  // Set the device appearance as keyboard (961).
  properties->appearance.ReplaceValue(961);  // DeviceChanged method called
  properties->appearance.set_valid(true);
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());
  EXPECT_EQ(961, devices[idx]->GetAppearance());
  // When discovery is over, the value should be invalidated.
  properties->appearance.set_valid(false);
  // DeviceChanged method called by NotifyPropertyChanged()
  properties->NotifyPropertyChanged(properties->appearance.name());
  EXPECT_EQ(3, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());
  EXPECT_EQ((int)BluetoothDevice::kAppearanceNotPresent,
            devices[idx]->GetAppearance());

  // Change the device appearance to mouse (962).
  properties->appearance.ReplaceValue(962);  // DeviceChanged method called
  properties->appearance.set_valid(true);
  EXPECT_EQ(4, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());
  EXPECT_EQ(962, devices[idx]->GetAppearance());
  // When discovery is over, the value should be invalidated.
  properties->appearance.set_valid(false);
  // DeviceChanged method called by NotifyPropertyChanged()
  properties->NotifyPropertyChanged(properties->appearance.name());
  EXPECT_EQ(5, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());
  EXPECT_EQ((int)BluetoothDevice::kAppearanceNotPresent,
            devices[idx]->GetAppearance());
}

TEST_F(BluetoothBlueZTest, DeviceTypebyAppearanceNotBluetoothClass) {
  // Test device type of a device with appearance but without bluetooth class.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);
  ASSERT_EQ(BluetoothDeviceType::COMPUTER, devices[idx]->GetDeviceType());

  // Install an observer; expect the DeviceChanged method to be called when
  // we change the appearance of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  // Let the device come without bluetooth_class.
  properties->bluetooth_class.ReplaceValue(0);  // DeviceChanged method called
  properties->appearance.ReplaceValue(0);       // DeviceChanged method called
  EXPECT_EQ(BluetoothDeviceType::UNKNOWN, devices[idx]->GetDeviceType());
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());

  // Set the device appearance as keyboard.
  properties->appearance.ReplaceValue(961);  // DeviceChanged method called
  properties->appearance.set_valid(true);
  EXPECT_EQ(BluetoothDeviceType::KEYBOARD, devices[idx]->GetDeviceType());
  EXPECT_EQ(3, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());
  // When discovery is over, the value should be invalidated.
  properties->appearance.set_valid(false);
  // DeviceChanged method called by NotifyPropertyChanged()
  properties->NotifyPropertyChanged(properties->appearance.name());
  EXPECT_EQ(4, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());
  EXPECT_EQ(BluetoothDeviceType::UNKNOWN, devices[idx]->GetDeviceType());

  // Change the device appearance to mouse.
  properties->appearance.ReplaceValue(962);  // DeviceChanged method called
  properties->appearance.set_valid(true);
  EXPECT_EQ(BluetoothDeviceType::MOUSE, devices[idx]->GetDeviceType());
  EXPECT_EQ(5, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());
  // When discovery is over, the value should be invalidated.
  properties->appearance.set_valid(false);
  // DeviceChanged method called by NotifyPropertyChanged()
  properties->NotifyPropertyChanged(properties->appearance.name());
  EXPECT_EQ(6, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());
  EXPECT_EQ(BluetoothDeviceType::UNKNOWN, devices[idx]->GetDeviceType());
}

TEST_F(BluetoothBlueZTest, DeviceNameChanged) {
  // Simulate a change of name of a device.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);
  ASSERT_EQ(bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[idx]->GetAddress());
  ASSERT_EQ(
      base::UTF8ToUTF16(bluez::FakeBluetoothDeviceClient::kPairedDeviceName),
      devices[idx]->GetNameForDisplay());

  // Install an observer; expect the DeviceChanged method to be called when
  // we change the alias of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  const std::string new_name("New Device Name");
  properties->name.ReplaceValue(new_name);

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());

  EXPECT_EQ(base::UTF8ToUTF16(new_name), devices[idx]->GetNameForDisplay());
}

TEST_F(BluetoothBlueZTest, DeviceAddressChanged) {
  // Simulate a change of address of a device.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);
  ASSERT_EQ(bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[idx]->GetAddress());
  ASSERT_EQ(
      base::UTF8ToUTF16(bluez::FakeBluetoothDeviceClient::kPairedDeviceName),
      devices[idx]->GetNameForDisplay());

  // Install an observer; expect the DeviceAddressChanged method to be called
  // when we change the alias of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  static const char* kNewAddress = "D9:1F:FC:11:22:33";
  properties->address.ReplaceValue(kNewAddress);

  EXPECT_EQ(1, observer.device_address_changed_count());
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());

  EXPECT_EQ(std::string(kNewAddress), devices[idx]->GetAddress());
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(BluetoothBlueZTest, DeviceBondedChanged) {
  // Simulate a change of bonded state of a device.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);
  ASSERT_EQ(bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[idx]->GetAddress());
  ASSERT_EQ(true, devices[idx]->IsBonded());

  // Install an observer; expect the DeviceBondedChanged method to be called
  // when we change the bonded state of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  properties->bonded.ReplaceValue(false);

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(1, observer.device_bonded_changed_count());
  EXPECT_FALSE(observer.device_new_bonded_status());
  EXPECT_EQ(devices[idx], observer.last_device());

  // Change the bonded state back to true to examine the consistent behavior of
  // DevicePairedChanged method.
  properties->bonded.ReplaceValue(true);

  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(2, observer.device_bonded_changed_count());
  EXPECT_TRUE(observer.device_new_bonded_status());
  EXPECT_EQ(devices[idx], observer.last_device());
}
#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
TEST_F(BluetoothBlueZTest, DevicePairedChanged) {
  // Simulate a change of paired state of a device.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);
  ASSERT_EQ(bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[idx]->GetAddress());
  ASSERT_EQ(true, devices[idx]->IsPaired());

  // Install an observer; expect the DevicePairedChanged method to be called
  // when we change the paired state of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  properties->paired.ReplaceValue(false);

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(1, observer.device_paired_changed_count());
  EXPECT_FALSE(observer.device_new_paired_status());
  EXPECT_EQ(devices[idx], observer.last_device());

  // Change the paired state back to true to examine the consistent behavior of
  // DevicePairedChanged method.
  properties->paired.ReplaceValue(true);

  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(2, observer.device_paired_changed_count());
  EXPECT_TRUE(observer.device_new_paired_status());
  EXPECT_EQ(devices[idx], observer.last_device());
}

TEST_F(BluetoothBlueZTest, DeviceMTUChanged) {
  // Simulate a change of MTU of a device.
  GetAdapter();

  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  // Install an observer; expect DeviceMTUChanged method to be called with the
  // updated MTU values.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(0, observer.device_mtu_changed_count());

  properties->mtu.ReplaceValue(258);
  EXPECT_EQ(1, observer.device_mtu_changed_count());
  EXPECT_EQ(258, observer.last_mtu_value());

  properties->mtu.ReplaceValue(128);
  EXPECT_EQ(2, observer.device_mtu_changed_count());
  EXPECT_EQ(128, observer.last_mtu_value());
}

TEST_F(BluetoothBlueZTest, DeviceAdvertisementReceived_LowEnergyDeviceAdded) {
  // Simulate reception of advertisement from a low energy device.
  GetAdapter();

  // Install an observer; expect DeviceAdvertisementReceived method to be called
  // with the EIR and RSSI.
  TestBluetoothAdapterObserver observer(adapter_);

  ASSERT_EQ(0, observer.device_advertisement_received_count());

  // A low energy device has a valid RSSI and EIR and should trigger the
  // advertisement callback.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  EXPECT_EQ(1, observer.device_advertisement_received_count());

  // A classic device does not have a valid RSSI and EIR and should not trigger
  // the advertisement callback.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kJustWorksPath));
  EXPECT_EQ(1, observer.device_advertisement_received_count());
}

TEST_F(BluetoothBlueZTest, DeviceAdvertisementReceived_PropertyChanged) {
  // Simulate reception of advertisement from a device.
  GetAdapter();

  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  // Install an observer; expect DeviceAdvertisementReceived method to be called
  // with the EIR and RSSI.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(0, observer.device_advertisement_received_count());

  properties->rssi.ReplaceValue(-73);
  EXPECT_EQ(1, observer.device_advertisement_received_count());

  std::vector<uint8_t> eir = {0x01, 0x02, 0x03};
  properties->eir.ReplaceValue(eir);
  EXPECT_EQ(2, observer.device_advertisement_received_count());
  EXPECT_EQ(eir, observer.device_eir());
}

TEST_F(BluetoothBlueZTest, DeviceConnectedStateChanged) {
  GetAdapter();

  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  // Install an observer; expect DeviceConnectedStateChanged method to be
  // called.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));

  // The device starts out disconnected.
  EXPECT_FALSE(device->IsConnected());

  properties->connected.ReplaceValue(true);
  EXPECT_EQ(1u, observer.device_connected_state_changed_values().size());
  EXPECT_TRUE(observer.device_connected_state_changed_values()[0]);

  properties->connected.ReplaceValue(false);
  EXPECT_EQ(2u, observer.device_connected_state_changed_values().size());
  EXPECT_FALSE(observer.device_connected_state_changed_values()[1]);
}
#endif

TEST_F(BluetoothBlueZTest, DeviceUuidsChanged) {
  // Simulate a change of advertised services of a device.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);
  ASSERT_EQ(bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[idx]->GetAddress());

  BluetoothDevice::UUIDSet uuids = devices[idx]->GetUUIDs();
  ASSERT_EQ(2U, uuids.size());
  ASSERT_TRUE(base::Contains(uuids, BluetoothUUID("1800")));
  ASSERT_TRUE(base::Contains(uuids, BluetoothUUID("1801")));

  // Install an observer; expect the DeviceChanged method to be called when
  // we change the class of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  std::vector<std::string> new_uuids;
  new_uuids.push_back(BluetoothUUID("1800").canonical_value());
  new_uuids.push_back(BluetoothUUID("1801").canonical_value());
  new_uuids.push_back("0000110c-0000-1000-8000-00805f9b34fb");
  new_uuids.push_back("0000110e-0000-1000-8000-00805f9b34fb");
  new_uuids.push_back("0000110a-0000-1000-8000-00805f9b34fb");

  properties->uuids.ReplaceValue(new_uuids);

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());

  // Fetching the value should give the new one.
  uuids = devices[idx]->GetUUIDs();
  EXPECT_EQ(5U, uuids.size());
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("1800")));
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("1801")));
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("110c")));
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("110e")));
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("110a")));
}

TEST_F(BluetoothBlueZTest, DeviceInquiryRSSIInvalidated) {
  // Simulate invalidation of inquiry RSSI of a device, as it occurs
  // when discovery is finished.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  // During discovery, rssi is a valid value (-75)
  properties->rssi.ReplaceValue(-75);
  properties->rssi.set_valid(true);

  ASSERT_EQ(-75, devices[idx]->GetInquiryRSSI().value());

  properties->rssi.ReplaceValue(INT8_MAX + 1);
  ASSERT_EQ(INT8_MAX, devices[idx]->GetInquiryRSSI().value());

  properties->rssi.ReplaceValue(INT8_MIN - 1);
  ASSERT_EQ(INT8_MIN, devices[idx]->GetInquiryRSSI().value());

  // Install an observer; expect the DeviceChanged method to be called when
  // we invalidate the RSSI of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  // When discovery is over, the value should be invalidated.
  properties->rssi.set_valid(false);
  properties->NotifyPropertyChanged(properties->rssi.name());

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());

  EXPECT_FALSE(devices[idx]->GetInquiryRSSI());
}

TEST_F(BluetoothBlueZTest, DeviceInquiryTxPowerInvalidated) {
  // Simulate invalidation of inquiry TxPower of a device, as it occurs
  // when discovery is finished.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  // During discovery, tx_power is a valid value (0)
  properties->tx_power.ReplaceValue(0);
  properties->tx_power.set_valid(true);

  ASSERT_EQ(0, devices[idx]->GetInquiryTxPower().value());

  properties->tx_power.ReplaceValue(INT8_MAX + 1);
  ASSERT_EQ(INT8_MAX, devices[idx]->GetInquiryTxPower().value());

  properties->tx_power.ReplaceValue(INT8_MIN - 1);
  ASSERT_EQ(INT8_MIN, devices[idx]->GetInquiryTxPower().value());

  // Install an observer; expect the DeviceChanged method to be called when
  // we invalidate the tx_power of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  // When discovery is over, the value should be invalidated.
  properties->tx_power.set_valid(false);
  properties->NotifyPropertyChanged(properties->tx_power.name());

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());

  EXPECT_FALSE(devices[idx]->GetInquiryTxPower());
}

TEST_F(BluetoothBlueZTest, ForgetDevice) {
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);
  ASSERT_EQ(bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[idx]->GetAddress());

  std::string address = devices[idx]->GetAddress();

  // Install an observer; expect the DeviceRemoved method to be called
  // with the device we remove.
  TestBluetoothAdapterObserver observer(adapter_);

  devices[idx]->Forget(base::DoNothing(), GetErrorCallback(base::DoNothing()));
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.device_removed_count());
  EXPECT_EQ(address, observer.last_device_address());

#if BUILDFLAG(IS_CHROMEOS)
  histogram_tester_.ExpectBucketCount("Bluetooth.ChromeOS.Forget.Result",
                                      device::ForgetResult::kSuccess, 1);
#endif

  // GetDevices shouldn't return the device either.
  devices = adapter_->GetDevices();
  ASSERT_EQ(1U, devices.size());
}

TEST_P(BluetoothBlueZTestP, ForgetUnpairedDevice) {
  GetAdapter();
  DiscoverDevices();

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kConnectUnpairableAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  // Connect the device so it becomes trusted and remembered.
  base::RunLoop run_loop;
  PerformConnect(
      device, nullptr,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  ASSERT_TRUE(device->IsConnected());
  ASSERT_FALSE(device->IsConnecting());

#if BUILDFLAG(IS_CHROMEOS)
  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kConnectUnpairablePath));
  ASSERT_TRUE(properties->trusted.value());
#endif

  // Install an observer; expect the DeviceRemoved method to be called
  // with the device we remove.
  TestBluetoothAdapterObserver observer(adapter_);

  device->Forget(base::DoNothing(), GetErrorCallback(base::DoNothing()));
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.device_removed_count());
  EXPECT_EQ(bluez::FakeBluetoothDeviceClient::kConnectUnpairableAddress,
            observer.last_device_address());

  // GetDevices shouldn't return the device either.
  device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kConnectUnpairableAddress);
  EXPECT_FALSE(device != nullptr);
}

TEST_P(BluetoothBlueZTestP, ConnectPairedDevice) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_TRUE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  // Connect without a pairing delegate; since the device is already Paired
  // this should succeed and the device should become connected.
  base::RunLoop run_loop;
  PerformConnect(
      device, nullptr,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

// Two changes for connecting, one for connected.
#if BUILDFLAG(IS_CHROMEOS)
  // One more for trusted after connecting.
  EXPECT_EQ(4, observer.device_changed_count());
#else
  EXPECT_EQ(3, observer.device_changed_count());
#endif
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
}

TEST_P(BluetoothBlueZTestP, ConnectUnpairableDevice) {
  GetAdapter();
  DiscoverDevices();

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kConnectUnpairableAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  // Connect without a pairing delegate; since the device does not require
  // pairing, this should succeed and the device should become connected.
  base::RunLoop run_loop;
  PerformConnect(
      device, nullptr,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

// Two changes for connecting, one for connected, and one for the reconnect mode
// (IsConnectable).
#if BUILDFLAG(IS_CHROMEOS)
  // One more for trusted after connection.
  EXPECT_EQ(5, observer.device_changed_count());
#else
  EXPECT_EQ(4, observer.device_changed_count());
#endif
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kConnectUnpairablePath));
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(properties->trusted.value());
#else
  EXPECT_FALSE(properties->trusted.value());
#endif

  // Verify is a HID device and is not connectable.
  BluetoothDevice::UUIDSet uuids = device->GetUUIDs();
  EXPECT_EQ(1U, uuids.size());
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("1124")));
  EXPECT_FALSE(device->IsConnectable());
}

TEST_P(BluetoothBlueZTestP, ConnectConnectedDevice) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_TRUE(device->IsPaired());

  {
    base::RunLoop run_loop;
    PerformConnect(
        device, nullptr,
        base::BindLambdaForTesting(
            [&run_loop](
                std::optional<BluetoothDevice::ConnectErrorCode> error) {
              EXPECT_FALSE(error.has_value());
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  ASSERT_TRUE(device->IsConnected());

  // Connect again; since the device is already Connected, this shouldn't do
  // anything to initiate the connection.
  TestBluetoothAdapterObserver observer(adapter_);

  {
    base::RunLoop run_loop;
    PerformConnect(
        device, nullptr,
        base::BindLambdaForTesting(
            [&run_loop](
                std::optional<BluetoothDevice::ConnectErrorCode> error) {
              EXPECT_FALSE(error.has_value());
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // The observer will be called because Connecting will toggle true and false.
#if BUILDFLAG(IS_CHROMEOS)
  // One more for trusted.
  EXPECT_EQ(3, observer.device_changed_count());
#else
  EXPECT_EQ(2, observer.device_changed_count());
#endif

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
}

TEST_P(BluetoothBlueZTestP, ConnectDeviceFails) {
  GetAdapter();
  DiscoverDevices();

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kLegacyAutopairAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  // Connect without a pairing delegate; since the device requires pairing,
  // this should fail with an error.
  base::RunLoop run_loop;
  PerformConnect(
      device, nullptr,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_EQ(BluetoothDevice::ERROR_FAILED, error);
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_EQ(2, observer.device_changed_count());

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
}

// Tests that discovery is unpaused if the device gets removed during a
// connection.
TEST_P(BluetoothBlueZTestP, RemoveDeviceDuringConnection) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);

  fake_bluetooth_device_client_->LeaveConnectionsPending();
  PerformConnect(
      device, nullptr,
      base::BindLambdaForTesting(
          [](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            FAIL() << "Connection callback should never be called.";
          }));

  EXPECT_FALSE(device->IsConnected());
  EXPECT_TRUE(device->IsConnecting());

  // Install an observer; expect the DeviceRemoved method to be called
  // with the device we remove.
  TestBluetoothAdapterObserver observer(adapter_);

  device->Forget(base::DoNothing(), GetErrorCallback(base::DoNothing()));
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.device_removed_count());
}

TEST_P(BluetoothBlueZTestP, DisconnectDevice) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);

#if BUILDFLAG(IS_CHROMEOS)
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));
  properties->type.ReplaceValue(BluetoothDeviceClient::kTypeBredr);
  properties->type.set_valid(true);
#endif  // BUILDFLAG(IS_CHROMEOS)

  ASSERT_TRUE(device != nullptr);
  ASSERT_TRUE(device->IsPaired());

  base::RunLoop run_loop;
  PerformConnect(
      device, nullptr,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  ASSERT_TRUE(device->IsConnected());
  ASSERT_FALSE(device->IsConnecting());

  // Disconnect the device, we should see the observer method fire and the
  // device get dropped.
  TestBluetoothAdapterObserver observer(adapter_);
  {
    base::RunLoop loop;
    device->Disconnect(GetCallback(loop.QuitClosure()),
                       GetErrorCallback(loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_FALSE(device->IsConnected());

#if BUILDFLAG(IS_CHROMEOS)
  histogram_tester_.ExpectBucketCount(
      "Bluetooth.ChromeOS.UserInitiatedDisconnect.Result",
      device::DisconnectResult::kSuccess, 1);
  histogram_tester_.ExpectBucketCount(
      "Bluetooth.ChromeOS.UserInitiatedDisconnect.Result.Classic",
      device::DisconnectResult::kSuccess, 1);
  histogram_tester_.ExpectBucketCount("Bluetooth.ChromeOS.DeviceDisconnect",
                                      device->GetDeviceType(), 1);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST_F(BluetoothBlueZTest, DisconnectUnconnectedDevice) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_TRUE(device->IsPaired());
  ASSERT_FALSE(device->IsConnected());

  // Disconnect the device, we should see the observer method fire and the
  // device get dropped.
  TestBluetoothAdapterObserver observer(adapter_);
  base::RunLoop loop;
  device->Disconnect(GetCallback(loop.QuitClosure()),
                     GetErrorCallback(loop.QuitClosure()));
  loop.Run();
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(0, observer.device_changed_count());

  EXPECT_FALSE(device->IsConnected());
}

TEST_F(BluetoothBlueZTest, PairTrustedDevice) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);
  GetAdapter();

  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::
                           kConnectedTrustedNotPairedDevicePath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::
                              kConnectedTrustedNotPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::
                               kConnectedTrustedNotPairedDevicePath));
  EXPECT_FALSE(properties->paired.value());
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(properties->trusted.value());
#else
  EXPECT_FALSE(properties->trusted.value());
#endif
  ASSERT_FALSE(device->IsPaired());

  // The |kConnectedTrustedNotPairedDevicePath| requests a passkey confirmation.
  // Obs.: This is the flow when CrOS triggers pairing with a iOS device.
  TestBluetoothAdapterObserver observer(adapter_);
  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, ConfirmPasskey)
      .WillOnce([](BluetoothDevice* device, uint32_t passkey) {
        ASSERT_NE(device, nullptr);
        EXPECT_EQ(123456U, passkey);
        device->ConfirmPairing();
      });
  adapter_->AddPairingDelegate(
      &pairing_delegate, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);
  base::RunLoop run_loop;
  device->Pair(
      &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  // Make sure the paired property has been set to true.
  properties = fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
      bluez::FakeBluetoothDeviceClient::kConnectedTrustedNotPairedDevicePath));
  EXPECT_TRUE(properties->paired.value());
}

TEST_F(BluetoothBlueZTest, PairAlreadyPairedDevice) {
  GetAdapter();

  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kPairedDevicePath));
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);

  // On the DBus level a device can be trusted but not paired.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));
  EXPECT_TRUE(properties->paired.value());
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(properties->trusted.value());
#else
  EXPECT_FALSE(properties->trusted.value());
#endif
  ASSERT_TRUE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);
  // For already paired devices a call to |Pair| should succeed without calling
  // the pairing delegate.
  StrictMock<MockPairingDelegate> pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  base::RunLoop run_loop;
  device->Pair(
      &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_P(BluetoothBlueZTestP, PairLegacyAutopair) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // The Legacy Autopair device requires no PIN or Passkey to pair because
  // the daemon provides 0000 to the device for us.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kLegacyAutopairAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  StrictMock<MockPairingDelegate> pairing_delegate;
  base::RunLoop run_loop;
  PerformConnect(
      device, &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  // Two changes for connecting, one change for connected, one for paired,
  // and one for the reconnect mode (IsConnectable).
#if BUILDFLAG(IS_CHROMEOS)
  // One more for bonded and two more for trusted (after pairing and connection)
  EXPECT_EQ(8, observer.device_changed_count());
#else
  EXPECT_EQ(5, observer.device_changed_count());
#endif
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Verify is a HID device and is connectable.
  BluetoothDevice::UUIDSet uuids = device->GetUUIDs();
  EXPECT_EQ(1U, uuids.size());
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("1124")));
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kLegacyAutopairPath));
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(properties->trusted.value());
#else
  EXPECT_FALSE(properties->trusted.value());
#endif
}

TEST_P(BluetoothBlueZTestP, PairDisplayPinCode) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Requires that we display a randomly generated PIN on the screen.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kDisplayPinCodeAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, DisplayPinCode(_, "123456")).Times(1);
  base::RunLoop run_loop;
  PerformConnect(
      device, &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  // Two changes for connecting, one change for connected, one for paired,
  // and one for the reconnect mode (IsConnectable).
#if BUILDFLAG(IS_CHROMEOS)
  // One more for bonded and two more for trusted (after pairing and connection)
  EXPECT_EQ(8, observer.device_changed_count());
#else
  EXPECT_EQ(5, observer.device_changed_count());
#endif

  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Verify is a HID device and is connectable.
  BluetoothDevice::UUIDSet uuids = device->GetUUIDs();
  EXPECT_EQ(1U, uuids.size());
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("1124")));
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kDisplayPinCodePath));
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(properties->trusted.value());
#else
  EXPECT_FALSE(properties->trusted.value());
#endif
}

TEST_P(BluetoothBlueZTestP, PairDisplayPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Requires that we display a randomly generated Passkey on the screen,
  // and notifies us as it's typed in.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kDisplayPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, DisplayPasskey(_, 123456U)).Times(1);
  testing::InSequence seq;
  for (int i = 0; i < 8; ++i) {
    EXPECT_CALL(pairing_delegate, KeysEntered(_, i));
  }
  base::RunLoop run_loop;
  PerformConnect(
      device, &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  // Two changes for connecting, one change for connected, one for paired,
  // and one for the reconnect mode (IsConnectable).
#if BUILDFLAG(IS_CHROMEOS)
  // One more for bonded and two for trusted (after pairing and connection)
  EXPECT_EQ(8, observer.device_changed_count());
#else
  EXPECT_EQ(5, observer.device_changed_count());
#endif
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Verify is a HID device.
  BluetoothDevice::UUIDSet uuids = device->GetUUIDs();
  EXPECT_EQ(1U, uuids.size());
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("1124")));

  // And usually not connectable.
  EXPECT_FALSE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kDisplayPasskeyPath));
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(properties->trusted.value());
#else
  EXPECT_FALSE(properties->trusted.value());
#endif
}

TEST_P(BluetoothBlueZTestP, PairRequestPinCode) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);
  GetAdapter();
  DiscoverDevices();

  // Requires that the user enters a PIN for them.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPinCodeAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, RequestPinCode)
      .WillOnce([](BluetoothDevice* device) { device->SetPinCode("1234"); });
  base::RunLoop run_loop;
  PerformConnect(
      device, &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  // Two changes for connecting, one change for connected, one for paired
#if BUILDFLAG(IS_CHROMEOS)
  // One more for bonded and two for trusted (after pairing and connection).
  EXPECT_EQ(7, observer.device_changed_count());
#else
  EXPECT_EQ(4, observer.device_changed_count());
#endif

  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Verify is not a HID device.
  BluetoothDevice::UUIDSet uuids = device->GetUUIDs();
  EXPECT_EQ(0U, uuids.size());

  // Non HID devices are always connectable.
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kRequestPinCodePath));
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(properties->trusted.value());
#else
  EXPECT_FALSE(properties->trusted.value());
#endif
}

TEST_P(BluetoothBlueZTestP, PairConfirmPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Requests that we confirm a displayed passkey.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kConfirmPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, ConfirmPasskey)
      .WillOnce([](BluetoothDevice* device, uint32_t passkey) {
        ASSERT_NE(device, nullptr);
        EXPECT_EQ(123456U, passkey);
        device->ConfirmPairing();
      });

  base::RunLoop run_loop;
  PerformConnect(
      device, &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  // Two changes for connecting, one change for connected, one for paired
#if BUILDFLAG(IS_CHROMEOS)
  // One more for bonded and two for trusted (after pairing and connection).
  EXPECT_EQ(7, observer.device_changed_count());
#else
  EXPECT_EQ(4, observer.device_changed_count());
#endif

  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Non HID devices are always connectable.
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kConfirmPasskeyPath));
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(properties->trusted.value());
#else
  EXPECT_FALSE(properties->trusted.value());
#endif
}

TEST_P(BluetoothBlueZTestP, PairRequestPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Requires that the user enters a Passkey, this would be some kind of
  // device that has a display, but doesn't use "just works" - maybe a car?
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, RequestPasskey)
      .WillOnce([](BluetoothDevice* device) {
        ASSERT_NE(device, nullptr);
        device->SetPasskey(1234);
      });

  base::RunLoop run_loop;
  PerformConnect(
      device, &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  // Two changes for connecting, one change for connected, one for paired
#if BUILDFLAG(IS_CHROMEOS)
  // One more for bonded and two for trusted (after pairing and connection).
  EXPECT_EQ(7, observer.device_changed_count());
#else
  EXPECT_EQ(4, observer.device_changed_count());
#endif

  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Non HID devices are always connectable.
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kRequestPasskeyPath));
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(properties->trusted.value());
#else
  EXPECT_FALSE(properties->trusted.value());
#endif
}

TEST_P(BluetoothBlueZTestP, PairJustWorks) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Uses just-works pairing, since this is an outgoing pairing, no delegate
  // interaction is required.
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kJustWorksAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  StrictMock<MockPairingDelegate> pairing_delegate;
  base::RunLoop run_loop;
  PerformConnect(
      device, &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  // Two changes for connecting, one change for connected, one for paired
#if BUILDFLAG(IS_CHROMEOS)
  // One more for bonded and two for trusted (after pairing and connection).
  EXPECT_EQ(7, observer.device_changed_count());
#else
  EXPECT_EQ(4, observer.device_changed_count());
#endif

  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Non HID devices are always connectable.
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kJustWorksPath));
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(properties->trusted.value());
#else
  EXPECT_FALSE(properties->trusted.value());
#endif
}

TEST_P(BluetoothBlueZTestP, PairUnpairableDeviceFails) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevice(bluez::FakeBluetoothDeviceClient::kUnconnectableDeviceAddress);

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kUnpairableDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  StrictMock<MockPairingDelegate> pairing_delegate;
  base::RunLoop run_loop;
  PerformConnect(
      device, &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_EQ(BluetoothDevice::ERROR_FAILED, error);
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_P(BluetoothBlueZTestP, PairingFails) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevice(bluez::FakeBluetoothDeviceClient::kVanishingDeviceAddress);

  // The vanishing device times out during pairing
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kVanishingDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  StrictMock<MockPairingDelegate> pairing_delegate;
  base::RunLoop run_loop;
  PerformConnect(
      device, &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_EQ(BluetoothDevice::ERROR_AUTH_TIMEOUT, error);
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_P(BluetoothBlueZTestP, PairingFailsAtConnection) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Everything seems to go according to plan with the unconnectable device;
  // it pairs, but then you can't make connections to it after.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kUnconnectableDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  StrictMock<MockPairingDelegate> pairing_delegate;
  base::RunLoop run_loop;
  PerformConnect(
      device, &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_EQ(BluetoothDevice::ERROR_FAILED, error);
            run_loop.Quit();
          }));
  run_loop.Run();

  // Two changes for connecting, one for paired.
  // The device should not be connected.
#if BUILDFLAG(IS_CHROMEOS)
  // One more for bonded and one for trusted after pairing
  EXPECT_EQ(5, observer.device_changed_count());
#else
  EXPECT_EQ(3, observer.device_changed_count());
#endif

  EXPECT_EQ(device, observer.last_device());

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Make sure the trusted property has been set to true still (since pairing
  // worked).
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kUnconnectableDevicePath));
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(properties->trusted.value());
#else
  EXPECT_FALSE(properties->trusted.value());
#endif
}

TEST_P(BluetoothBlueZTestP, PairingRejectedAtPinCode) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Reject the pairing after we receive a request for the PIN code.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPinCodeAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, RequestPinCode)
      .WillOnce([](BluetoothDevice* device) {
        ASSERT_NE(device, nullptr);
        ScheduleAsynchronousRejectPairing(device);
      });
  base::RunLoop run_loop;
  PerformConnect(
      device, &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_EQ(BluetoothDevice::ERROR_AUTH_REJECTED, error);
            run_loop.Quit();
          }));
  run_loop.Run();

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_P(BluetoothBlueZTestP, PairingCancelledAtPinCode) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Cancel the pairing after we receive a request for the PIN code.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPinCodeAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, RequestPinCode)
      .WillOnce([](BluetoothDevice* device) {
        ASSERT_NE(device, nullptr);
        ScheduleAsynchronousCancelPairing(device);
      });
  base::RunLoop run_loop;
  PerformConnect(
      device, &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, error);
            run_loop.Quit();
          }));
  run_loop.Run();

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_P(BluetoothBlueZTestP, PairingRejectedAtPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Reject the pairing after we receive a request for the passkey.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, RequestPasskey)
      .WillOnce([](BluetoothDevice* device) {
        ASSERT_NE(device, nullptr);
        ScheduleAsynchronousRejectPairing(device);
      });
  base::RunLoop run_loop;
  PerformConnect(
      device, &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_EQ(BluetoothDevice::ERROR_AUTH_REJECTED, error);
            run_loop.Quit();
          }));
  run_loop.Run();

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_P(BluetoothBlueZTestP, PairingCancelledAtPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Cancel the pairing after we receive a request for the passkey.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, RequestPasskey)
      .WillOnce([](BluetoothDevice* device) {
        ASSERT_NE(device, nullptr);
        ScheduleAsynchronousCancelPairing(device);
      });
  base::RunLoop run_loop;
  PerformConnect(
      device, &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, error);
            run_loop.Quit();
          }));
  run_loop.Run();

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_P(BluetoothBlueZTestP, PairingRejectedAtConfirmation) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Reject the pairing after we receive a request for passkey confirmation.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kConfirmPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, ConfirmPasskey)
      .WillOnce([](BluetoothDevice* device, uint32_t passkey) {
        ASSERT_NE(device, nullptr);
        EXPECT_EQ(passkey, 123456U);
        ScheduleAsynchronousRejectPairing(device);
      });
  base::RunLoop run_loop;
  PerformConnect(
      device, &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_EQ(BluetoothDevice::ERROR_AUTH_REJECTED, error);
            run_loop.Quit();
          }));
  run_loop.Run();

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_P(BluetoothBlueZTestP, PairingCancelledAtConfirmation) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Cancel the pairing after we receive a request for the passkey.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kConfirmPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, ConfirmPasskey)
      .WillOnce([](BluetoothDevice* device, uint32_t passkey) {
        ASSERT_NE(device, nullptr);
        EXPECT_EQ(passkey, 123456U);
        ScheduleAsynchronousCancelPairing(device);
      });
  base::RunLoop run_loop;
  PerformConnect(
      device, &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, error);
            run_loop.Quit();
          }));
  run_loop.Run();

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_P(BluetoothBlueZTestP, PairingCancelledInFlight) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Cancel the pairing while we're waiting for the remote host.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kLegacyAutopairAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  StrictMock<MockPairingDelegate> pairing_delegate;
  bool callback_called = false;
  base::RunLoop run_loop;
  PerformConnect(
      device, &pairing_delegate,
      base::BindLambdaForTesting(
          [&](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            callback_called = true;
            EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, error);
            run_loop.Quit();
          }));

  EXPECT_FALSE(callback_called);
  EXPECT_TRUE(device->IsConnecting());

  // Cancel the pairing.
  device->CancelPairing();
  run_loop.Run();

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothBlueZTest, IncomingPairRequestPinCode) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  StrictMock<MockPairingDelegate> pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Requires that we provide a PIN code.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kRequestPinCodePath));
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPinCodeAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  EXPECT_CALL(pairing_delegate, RequestPinCode)
      .WillOnce([](BluetoothDevice* device) {
        ASSERT_NE(device, nullptr);
        device->SetPinCode("1234");
      });

  base::RunLoop run_loop;
  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kRequestPinCodePath),
      true, base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }),
      base::BindLambdaForTesting([&run_loop](const std::string& error_name,
                                             const std::string& error_message) {
        ADD_FAILURE() << "Pairing unexpectedly failed: " << error_message;
        run_loop.Quit();
      }));
  run_loop.Run();

  // One change for paired.
#if BUILDFLAG(IS_CHROMEOS)
  // One more for bonded , and one for trusted.
  EXPECT_EQ(3, observer.device_changed_count());
#else
  EXPECT_EQ(1, observer.device_changed_count());
#endif

  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsPaired());

#if BUILDFLAG(IS_CHROMEOS)
  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kRequestPinCodePath));
  ASSERT_TRUE(properties->trusted.value());
#endif

  // No pairing context should remain on the device.
  BluetoothDeviceBlueZ* device_bluez =
      static_cast<BluetoothDeviceBlueZ*>(device);
  EXPECT_TRUE(device_bluez->GetPairing() == nullptr);
}

TEST_F(BluetoothBlueZTest, IncomingPairConfirmPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  StrictMock<MockPairingDelegate> pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Requests that we confirm a displayed passkey.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kConfirmPasskeyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kConfirmPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  EXPECT_CALL(pairing_delegate, ConfirmPasskey)
      .WillOnce([](BluetoothDevice* device, uint32_t passkey) {
        EXPECT_EQ(passkey, 123456U);
        device->ConfirmPairing();
      });

  base::RunLoop run_loop;
  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kConfirmPasskeyPath),
      true, base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }),
      base::BindLambdaForTesting([&run_loop](const std::string& error_name,
                                             const std::string& error_message) {
        ADD_FAILURE() << "Pairing unexpectedly failed: " << error_message;
        run_loop.Quit();
      }));
  run_loop.Run();

  // One change for paired.
#if BUILDFLAG(IS_CHROMEOS)
  // One more for bonded, and one for trusted
  EXPECT_EQ(3, observer.device_changed_count());
#else
  EXPECT_EQ(1, observer.device_changed_count());
#endif

  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsPaired());

#if BUILDFLAG(IS_CHROMEOS)
  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kConfirmPasskeyPath));
  ASSERT_TRUE(properties->trusted.value());
#endif

  // No pairing context should remain on the device.
  BluetoothDeviceBlueZ* device_bluez =
      static_cast<BluetoothDeviceBlueZ*>(device);
  EXPECT_TRUE(device_bluez->GetPairing() == nullptr);
}

TEST_F(BluetoothBlueZTest, IncomingPairRequestPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  StrictMock<MockPairingDelegate> pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Requests that we provide a Passkey.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kRequestPasskeyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  EXPECT_CALL(pairing_delegate, RequestPasskey)
      .WillOnce([](BluetoothDevice* device) {
        ASSERT_NE(device, nullptr);
        device->SetPasskey(1234);
      });

  base::RunLoop run_loop;
  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kRequestPasskeyPath),
      true, base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }),
      base::BindLambdaForTesting([&run_loop](const std::string& error_name,
                                             const std::string& error_message) {
        ADD_FAILURE() << "Pairing unexpectedly failed: " << error_message;
        run_loop.Quit();
      }));
  run_loop.Run();

  // One change for paired.
#if BUILDFLAG(IS_CHROMEOS)
  // One more for bonded, and one for trusted.
  EXPECT_EQ(3, observer.device_changed_count());
#else
  EXPECT_EQ(1, observer.device_changed_count());
#endif

  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsPaired());

#if BUILDFLAG(IS_CHROMEOS)
  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kRequestPasskeyPath));
  ASSERT_TRUE(properties->trusted.value());
#endif

  // No pairing context should remain on the device.
  BluetoothDeviceBlueZ* device_bluez =
      static_cast<BluetoothDeviceBlueZ*>(device);
  EXPECT_TRUE(device_bluez->GetPairing() == nullptr);
}

TEST_F(BluetoothBlueZTest, IncomingPairJustWorks) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  StrictMock<MockPairingDelegate> pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Uses just-works pairing so, sinec this an incoming pairing, require
  // authorization from the user.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kJustWorksPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kJustWorksAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  EXPECT_CALL(pairing_delegate, AuthorizePairing)
      .WillOnce([](BluetoothDevice* device) {
        ASSERT_NE(device, nullptr);
        device->ConfirmPairing();
      });

  base::RunLoop run_loop;
  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kJustWorksPath), true,
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }),
      base::BindLambdaForTesting([&run_loop](const std::string& error_name,
                                             const std::string& error_message) {
        ADD_FAILURE() << "Pairing unexpectedly failed: " << error_message;
        run_loop.Quit();
      }));
  run_loop.Run();

  // One change for paired
#if BUILDFLAG(IS_CHROMEOS)
  // One more for bonded, and one for trusted.
  EXPECT_EQ(3, observer.device_changed_count());
#else
  EXPECT_EQ(1, observer.device_changed_count());
#endif

  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsPaired());

#if BUILDFLAG(IS_CHROMEOS)
  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kJustWorksPath));
  ASSERT_TRUE(properties->trusted.value());
#endif

  // No pairing context should remain on the device.
  BluetoothDeviceBlueZ* device_bluez =
      static_cast<BluetoothDeviceBlueZ*>(device);
  EXPECT_TRUE(device_bluez->GetPairing() == nullptr);
}

TEST_F(BluetoothBlueZTest, IncomingPairRequestPinCodeWithoutDelegate) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  // Requires that we provide a PIN Code, without a pairing delegate,
  // that will be rejected.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kRequestPinCodePath));
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPinCodeAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);
  base::RunLoop loop;
  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kRequestPinCodePath),
      true, GetCallback(loop.QuitClosure()),
      base::BindLambdaForTesting(
          [&loop, this](const std::string& error_name,
                        const std::string& error_message) {
            DBusErrorCallback(error_name, error_message);
            loop.Quit();
          }));

  loop.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(bluetooth_device::kErrorAuthenticationRejected, last_client_error_);

  // No changes should be observer.
  EXPECT_EQ(0, observer.device_changed_count());

  EXPECT_FALSE(device->IsPaired());

  // No pairing context should remain on the device.
  BluetoothDeviceBlueZ* device_bluez =
      static_cast<BluetoothDeviceBlueZ*>(device);
  EXPECT_TRUE(device_bluez->GetPairing() == nullptr);
}

TEST_F(BluetoothBlueZTest, IncomingPairConfirmPasskeyWithoutDelegate) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  // Requests that we confirm a displayed passkey, without a pairing delegate,
  // that will be rejected.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kConfirmPasskeyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kConfirmPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  base::RunLoop loop;
  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kConfirmPasskeyPath),
      true, GetCallback(loop.QuitClosure()),
      base::BindLambdaForTesting(
          [&loop, this](const std::string& error_name,
                        const std::string& error_message) {
            DBusErrorCallback(error_name, error_message);
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(bluetooth_device::kErrorAuthenticationRejected, last_client_error_);

  // No changes should be observer.
  EXPECT_EQ(0, observer.device_changed_count());

  EXPECT_FALSE(device->IsPaired());

  // No pairing context should remain on the device.
  BluetoothDeviceBlueZ* device_bluez =
      static_cast<BluetoothDeviceBlueZ*>(device);
  EXPECT_TRUE(device_bluez->GetPairing() == nullptr);
}

TEST_F(BluetoothBlueZTest, IncomingPairRequestPasskeyWithoutDelegate) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  // Requests that we provide a displayed passkey, without a pairing delegate,
  // that will be rejected.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kRequestPasskeyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);
  base::RunLoop loop;
  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kRequestPasskeyPath),
      true, GetCallback(loop.QuitClosure()),
      base::BindLambdaForTesting(
          [&loop, this](const std::string& error_name,
                        const std::string& error_message) {
            DBusErrorCallback(error_name, error_message);
            loop.Quit();
          }));

  loop.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(bluetooth_device::kErrorAuthenticationRejected, last_client_error_);

  // No changes should be observer.
  EXPECT_EQ(0, observer.device_changed_count());

  EXPECT_FALSE(device->IsPaired());

  // No pairing context should remain on the device.
  BluetoothDeviceBlueZ* device_bluez =
      static_cast<BluetoothDeviceBlueZ*>(device);
  EXPECT_TRUE(device_bluez->GetPairing() == nullptr);
}

TEST_F(BluetoothBlueZTest, IncomingPairJustWorksWithoutDelegate) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  // Uses just-works pairing and thus requires authorization for incoming
  // pairings, without a pairing delegate, that will be rejected.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kJustWorksPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kJustWorksAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);
  base::RunLoop loop;
  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kJustWorksPath), true,
      GetCallback(base::DoNothing()),
      base::BindLambdaForTesting(
          [&loop, this](const std::string& error_name,
                        const std::string& error_message) {
            DBusErrorCallback(error_name, error_message);
            loop.Quit();
          }));

  loop.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(bluetooth_device::kErrorAuthenticationRejected, last_client_error_);

  // No changes should be observer.
  EXPECT_EQ(0, observer.device_changed_count());

  EXPECT_FALSE(device->IsPaired());

  // No pairing context should remain on the device.
  BluetoothDeviceBlueZ* device_bluez =
      static_cast<BluetoothDeviceBlueZ*>(device);
  EXPECT_TRUE(device_bluez->GetPairing() == nullptr);
}

TEST_F(BluetoothBlueZTest, RemovePairingDelegateDuringPairing) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  StrictMock<MockPairingDelegate> pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Requests that we provide a Passkey.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kRequestPasskeyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  EXPECT_CALL(pairing_delegate, RequestPasskey)
      .WillOnce([&](BluetoothDevice* device) {
        ASSERT_NE(device, nullptr);
        // A pairing context should now be set on the device.
        BluetoothDeviceBlueZ* device_bluez =
            static_cast<BluetoothDeviceBlueZ*>(device);
        EXPECT_NE(device_bluez->GetPairing(), nullptr);

        // Removing the pairing delegate should remove that pairing context.
        adapter_->RemovePairingDelegate(&pairing_delegate);
        EXPECT_EQ(device_bluez->GetPairing(), nullptr);

        // This passkey won't be used because of the delegate removal above.
        device->SetPasskey(111u);
      });

  base::RunLoop run_loop;
  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kRequestPasskeyPath),
      true, base::BindLambdaForTesting([&run_loop]() {
        ADD_FAILURE() << "Pairing unexpectedly succeeded.";
        run_loop.Quit();
      }),
      base::BindLambdaForTesting([&run_loop](const std::string& error_name,
                                             const std::string& error_message) {
        EXPECT_EQ("org.bluez.Error.AuthenticationCanceled", error_name);
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(0, observer.device_changed_count());

  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothBlueZTest, DeviceId) {
  GetAdapter();

  // Use the built-in paired device for this test, grab its Properties
  // structure so we can adjust the underlying modalias property.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  ASSERT_TRUE(device != nullptr);
  ASSERT_TRUE(properties != nullptr);

  // Valid USB IF-assigned identifier.
  ASSERT_EQ("usb:v05ACp030Dd0306", properties->modalias.value());

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_USB, device->GetVendorIDSource());
  EXPECT_EQ(0x05ac, device->GetVendorID());
  EXPECT_EQ(0x030d, device->GetProductID());
  EXPECT_EQ(0x0306, device->GetDeviceID());

  // Valid Bluetooth SIG-assigned identifier.
  properties->modalias.ReplaceValue("bluetooth:v00E0p2400d0400");

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_BLUETOOTH, device->GetVendorIDSource());
  EXPECT_EQ(0x00e0, device->GetVendorID());
  EXPECT_EQ(0x2400, device->GetProductID());
  EXPECT_EQ(0x0400, device->GetDeviceID());

  // Invalid USB IF-assigned identifier.
  properties->modalias.ReplaceValue("usb:x00E0p2400d0400");

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_UNKNOWN, device->GetVendorIDSource());
  EXPECT_EQ(0, device->GetVendorID());
  EXPECT_EQ(0, device->GetProductID());
  EXPECT_EQ(0, device->GetDeviceID());

  // Invalid Bluetooth SIG-assigned identifier.
  properties->modalias.ReplaceValue("bluetooth:x00E0p2400d0400");

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_UNKNOWN, device->GetVendorIDSource());
  EXPECT_EQ(0, device->GetVendorID());
  EXPECT_EQ(0, device->GetProductID());
  EXPECT_EQ(0, device->GetDeviceID());

  // Unknown vendor specification identifier.
  properties->modalias.ReplaceValue("chrome:v00E0p2400d0400");

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_UNKNOWN, device->GetVendorIDSource());
  EXPECT_EQ(0, device->GetVendorID());
  EXPECT_EQ(0, device->GetProductID());
  EXPECT_EQ(0, device->GetDeviceID());
}

TEST_F(BluetoothBlueZTest, GetConnectionInfoForDisconnectedDevice) {
  GetAdapter();
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);

  // Calling GetConnectionInfo for an unconnected device should return a result
  // in which all fields are filled with BluetoothDevice::kUnknownPower.
  BluetoothDevice::ConnectionInfo conn_info(0, 0, 0);
  device->GetConnectionInfo(base::BindOnce(&SaveConnectionInfo, &conn_info));
  int unknown_power = BluetoothDevice::kUnknownPower;
  EXPECT_NE(0, unknown_power);
  EXPECT_EQ(unknown_power, conn_info.rssi);
  EXPECT_EQ(unknown_power, conn_info.transmit_power);
  EXPECT_EQ(unknown_power, conn_info.max_transmit_power);
}

TEST_P(BluetoothBlueZTestP, GetConnectionInfoForConnectedDevice) {
  GetAdapter();
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);

  base::RunLoop run_loop;
  PerformConnect(
      device, nullptr,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_TRUE(device->IsConnected());

  // Calling GetConnectionInfo for a connected device should return valid
  // results.
  fake_bluetooth_device_client_->UpdateConnectionInfo(-10, 3, 4);
  BluetoothDevice::ConnectionInfo conn_info;
  device->GetConnectionInfo(base::BindOnce(&SaveConnectionInfo, &conn_info));
  EXPECT_EQ(-10, conn_info.rssi);
  EXPECT_EQ(3, conn_info.transmit_power);
  EXPECT_EQ(4, conn_info.max_transmit_power);
}

TEST_F(BluetoothBlueZTest, GetDiscoverableTimeout) {
  constexpr base::TimeDelta kShortDiscoverableTimeout = base::Seconds(30);
  constexpr base::TimeDelta kLongDiscoverableTimeout = base::Seconds(240);
  GetAdapter();
  BluetoothAdapterBlueZ* adapter_bluez =
      static_cast<BluetoothAdapterBlueZ*>(adapter_.get());

  fake_bluetooth_adapter_client_->SetDiscoverableTimeout(
      kShortDiscoverableTimeout);
  EXPECT_EQ(kShortDiscoverableTimeout, adapter_bluez->GetDiscoverableTimeout());

  fake_bluetooth_adapter_client_->SetDiscoverableTimeout(
      kLongDiscoverableTimeout);
  EXPECT_EQ(kLongDiscoverableTimeout, adapter_bluez->GetDiscoverableTimeout());
}

// Verifies Shutdown shuts down the adapter as expected.
TEST_F(BluetoothBlueZTest, Shutdown) {
  // Set up adapter. Set powered & discoverable, start discovery.
  GetAdapter();
  fake_bluetooth_adapter_client_->SetUUIDs(
      {kGapUuid, kGattUuid, kPnpUuid, kHeadsetUuid});
  EXPECT_TRUE(SetPoweredBlocking(true));

  {
    base::RunLoop loop;
    adapter_->SetDiscoverable(true, GetCallback(loop.QuitClosure()),
                              GetErrorCallback(loop.QuitClosure()));
    loop.Run();
  }

  auto observer = std::make_unique<TestBluetoothAdapterObserver>(adapter_);
  base::test::RepeatingTestFuture<void> discoverying_changed;
  observer->RegisterDiscoveringChangedWatcher(
      discoverying_changed.GetCallback());

  discovery_sessions_.push_back(StartDiscoverySessionBlocking());

  ASSERT_EQ(1, callback_count_);
  ASSERT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  StrictMock<MockPairingDelegate> pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Validate running adapter state.
  discoverying_changed.Take();
  EXPECT_NE("", adapter_->GetAddress());
  EXPECT_NE("", adapter_->GetName());
  EXPECT_EQ(4U, adapter_->GetUUIDs().size());
  EXPECT_TRUE(adapter_->IsInitialized());
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_TRUE(adapter_->IsDiscoverable());
  EXPECT_TRUE(IsAdapterDiscovering());
  EXPECT_EQ(2U, adapter_->GetDevices().size());
  EXPECT_NE(nullptr,
            adapter_->GetDevice(
                bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress));
  EXPECT_NE(dbus::ObjectPath(""),
            static_cast<BluetoothAdapterBlueZ*>(adapter_.get())->object_path());

  // Shutdown
  adapter_->Shutdown();

  // Validate post shutdown state by calling all BluetoothAdapterBlueZ
  // members, in declaration order:
  // DeleteOnCorrectThread omitted as we don't want to delete in this test.
  // ~TestBluetoothAdapterObserver calls RemoveObserver.
  observer.reset();

  EXPECT_EQ("", adapter_->GetAddress());
  EXPECT_EQ("", adapter_->GetName());
  EXPECT_EQ(0U, adapter_->GetUUIDs().size());

  {
    base::RunLoop loop;
    adapter_->SetName("", GetCallback(loop.QuitClosure()),
                      GetErrorCallback(loop.QuitClosure()));
    loop.Run();
  }
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_--) << "SetName error";

  EXPECT_TRUE(adapter_->IsInitialized());
  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());

  EXPECT_FALSE(SetPoweredBlocking(true));

  EXPECT_FALSE(adapter_->IsDiscoverable());

  {
    base::RunLoop loop;
    adapter_->SetDiscoverable(true, GetCallback(loop.QuitClosure()),
                              GetErrorCallback(loop.QuitClosure()));
    loop.Run();
  }
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_--) << "SetDiscoverable error";

  EXPECT_FALSE(IsAdapterDiscovering());
  // CreateRfcommService will DCHECK after Shutdown().
  // CreateL2capService will DCHECK after Shutdown().

  BluetoothAdapterBlueZ* adapter_bluez =
      static_cast<BluetoothAdapterBlueZ*>(adapter_.get());
  EXPECT_EQ(nullptr, adapter_bluez->GetDeviceWithPath(dbus::ObjectPath("")));

  // Notify methods presume objects exist that are owned by the adapter and
  // destroyed in Shutdown(). Mocks are not attempted here that won't exist,
  // as verified below by EXPECT_EQ(0U, adapter_->GetDevices().size());
  // NotifyDeviceChanged
  // NotifyGattServiceAdded
  // NotifyGattServiceRemoved
  // NotifyGattServiceChanged
  // NotifyGattDiscoveryComplete
  // NotifyGattCharacteristicAdded
  // NotifyGattCharacteristicRemoved
  // NotifyGattDescriptorAdded
  // NotifyGattDescriptorRemoved
  // NotifyGattCharacteristicValueChanged
  // NotifyGattDescriptorValueChanged

  EXPECT_EQ(dbus::ObjectPath(""), adapter_bluez->object_path());

  adapter_profile_ = nullptr;

  FakeBluetoothProfileServiceProviderDelegate profile_delegate;
  {
    base::RunLoop loop;

    adapter_bluez->UseProfile(
        BluetoothUUID(), dbus::ObjectPath(""),
        bluez::BluetoothProfileManagerClient::Options(), &profile_delegate,
        base::BindLambdaForTesting(
            [&loop, this](BluetoothAdapterProfileBlueZ* profile) {
              ProfileRegisteredCallback(profile);
              loop.Quit();
            }),
        base::BindLambdaForTesting(
            [&loop, this](const std::string& error_message) {
              ErrorCompletionCallback(error_message);
              loop.Quit();
            }));
    loop.Run();
  }

  EXPECT_FALSE(adapter_profile_) << "UseProfile error";
  EXPECT_EQ(0, callback_count_) << "UseProfile error";
  EXPECT_EQ(1, error_callback_count_--) << "UseProfile error";

  // Protected and private methods:

  adapter_bluez->RemovePairingDelegateInternal(&pairing_delegate);
  // AdapterAdded() invalid post Shutdown because it calls SetAdapter.
  adapter_bluez->AdapterRemoved(dbus::ObjectPath("x"));
  adapter_bluez->AdapterPropertyChanged(dbus::ObjectPath("x"), "");
  adapter_bluez->DeviceAdded(dbus::ObjectPath(""));
  adapter_bluez->DeviceRemoved(dbus::ObjectPath(""));
  adapter_bluez->DevicePropertyChanged(dbus::ObjectPath(""), "");
  adapter_bluez->InputPropertyChanged(dbus::ObjectPath(""), "");
  // bluez::BluetoothAgentServiceProvider::Delegate omitted, dbus will be
  // shutdown,
  //   with the exception of Released.
  adapter_bluez->Released();

  adapter_bluez->OnRegisterAgent();
  adapter_bluez->OnRegisterAgentError("", "");
  adapter_bluez->OnRequestDefaultAgent();
  adapter_bluez->OnRequestDefaultAgentError("", "");

  // GetPairing will DCHECK after Shutdown().
  // SetAdapter will DCHECK after Shutdown().
  // SetDefaultAdapterName will DCHECK after Shutdown().
  adapter_bluez->NotifyAdapterPoweredChanged(false);
  adapter_bluez->DiscoverableChanged(false);
  adapter_bluez->DiscoveringChanged(false);
  adapter_bluez->PresentChanged(false);

  {
    base::RunLoop loop;
    adapter_bluez->OnSetDiscoverable(GetCallback(loop.QuitClosure()),
                                     GetErrorCallback(loop.QuitClosure()),
                                     true);
    loop.Run();
  }
  EXPECT_EQ(0, callback_count_) << "OnSetDiscoverable error";
  EXPECT_EQ(1, error_callback_count_--) << "OnSetDiscoverable error";

  {
    base::RunLoop loop;
    adapter_bluez->OnPropertyChangeCompleted(
        GetCallback(loop.QuitClosure()), GetErrorCallback(loop.QuitClosure()),
        true);
    loop.Run();
  }
  EXPECT_EQ(0, callback_count_) << "OnPropertyChangeCompleted error";
  EXPECT_EQ(1, error_callback_count_--) << "OnPropertyChangeCompleted error";

  base::test::TestFuture<void> error_future;
  StrictMock<base::MockCallback<BluetoothAdapter::DiscoverySessionCallback>>
      success_callback;
  adapter_bluez->StartDiscoverySession(/*client_name=*/std::string(),
                                       success_callback.Get(),
                                       error_future.GetCallback());
  EXPECT_TRUE(error_future.Wait());
  error_future.Clear();

  // OnStartDiscovery tested in Shutdown_OnStartDiscovery
  // OnStartDiscoveryError tested in Shutdown_OnStartDiscoveryError

  adapter_profile_ = nullptr;

  // OnRegisterProfile SetProfileDelegate, OnRegisterProfileError, require
  // UseProfile to be set first, do so again here just before calling them.
  {
    base::RunLoop loop;
    adapter_bluez->UseProfile(
        BluetoothUUID(), dbus::ObjectPath(""),
        bluez::BluetoothProfileManagerClient::Options(), &profile_delegate,
        base::BindLambdaForTesting(
            [&loop, this](BluetoothAdapterProfileBlueZ* profile) {
              ProfileRegisteredCallback(profile);
              loop.Quit();
            }),
        base::BindLambdaForTesting(
            [&loop, this](const std::string& error_message) {
              ErrorCompletionCallback(error_message);
              loop.Quit();
            }));
    loop.Run();
  }
  EXPECT_FALSE(adapter_profile_) << "UseProfile error";
  EXPECT_EQ(0, callback_count_) << "UseProfile error";
  EXPECT_EQ(1, error_callback_count_--) << "UseProfile error";

  {
    base::RunLoop loop;
    adapter_bluez->SetProfileDelegate(
        BluetoothUUID(), dbus::ObjectPath(""), &profile_delegate,
        base::BindLambdaForTesting(
            [&loop, this](BluetoothAdapterProfileBlueZ* profile) {
              ProfileRegisteredCallback(profile);
              loop.Quit();
            }),
        base::BindLambdaForTesting(
            [&loop, this](const std::string& error_message) {
              ErrorCompletionCallback(error_message);
              loop.Quit();
            }));
    loop.Run();
  }
  EXPECT_EQ(0, callback_count_) << "SetProfileDelegate error";
  EXPECT_EQ(1, error_callback_count_--) << "SetProfileDelegate error";

  adapter_bluez->OnRegisterProfileError(BluetoothUUID(), "", "");
  EXPECT_EQ(0, callback_count_) << "OnRegisterProfileError error";
  EXPECT_EQ(0, error_callback_count_) << "OnRegisterProfileError error";

  // From BluetoothAdapater:

  adapter_bluez->StartDiscoverySession(/*client_name=*/std::string(),
                                       success_callback.Get(),
                                       error_future.GetCallback());
  EXPECT_TRUE(error_future.Wait());

  EXPECT_EQ(0U, adapter_->GetDevices().size());
  EXPECT_EQ(nullptr,
            adapter_->GetDevice(
                bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress));
  StrictMock<MockPairingDelegate> pairing_delegate2;
  adapter_->AddPairingDelegate(
      &pairing_delegate2, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);
  adapter_->RemovePairingDelegate(&pairing_delegate2);
}

TEST_F(BluetoothBlueZTest, StartDiscovery_DiscoveringStopped_StartAgain) {
  GetAdapter();
  BluetoothAdapterBlueZ* adapter_bluez =
      static_cast<BluetoothAdapterBlueZ*>(adapter_.get());
  {
    base::RunLoop loop;
    adapter_->StartDiscoverySession(
        /*client_name=*/std::string(),
        base::BindLambdaForTesting(
            [&](std::unique_ptr<BluetoothDiscoverySession> session) {
              loop.Quit();
            }),
        base::BindOnce([]() {
          ADD_FAILURE() << "Unexpected discovery session start error.";
        }));
    loop.Run();
  }
  adapter_bluez->DiscoveringChanged(false);
  {
    base::RunLoop loop;
    adapter_->StartDiscoverySession(
        /*client_name=*/std::string(),
        base::BindLambdaForTesting(
            [&](std::unique_ptr<BluetoothDiscoverySession> session) {
              loop.Quit();
            }),
        base::BindOnce([]() {
          ADD_FAILURE() << "Unexpected discovery session start error.";
        }));
    loop.Run();
  }
}

// Verifies post-Shutdown of discovery sessions and OnStartDiscovery.
TEST_F(BluetoothBlueZTest, Shutdown_OnStartDiscovery) {
  const int kNumberOfDiscoverySessions = 10;
  GetAdapter();
  BluetoothAdapterBlueZ* adapter_bluez =
      static_cast<BluetoothAdapterBlueZ*>(adapter_.get());

  base::RunLoop loop;
  for (int i = 0; i < kNumberOfDiscoverySessions; i++) {
    {
      adapter_bluez->StartDiscoverySession(
          /*client_name=*/std::string(),
          base::BindLambdaForTesting(
              [&loop, this](std::unique_ptr<BluetoothDiscoverySession>
                                discovery_session) {
                DiscoverySessionCallback(std::move(discovery_session));
                loop.Quit();
              }),
          GetErrorCallback(loop.QuitClosure()));
    }
  }
  adapter_->Shutdown();
  loop.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(kNumberOfDiscoverySessions, error_callback_count_);
}

// Verifies post-Shutdown of discovery sessions and OnStartDiscoveryError.
TEST_F(BluetoothBlueZTest, Shutdown_OnStartDiscoveryError) {
  const int kNumberOfDiscoverySessions = 10;
  GetAdapter();
  BluetoothAdapterBlueZ* adapter_bluez =
      static_cast<BluetoothAdapterBlueZ*>(adapter_.get());

  base::RunLoop loop;
  for (int i = 0; i < kNumberOfDiscoverySessions; i++) {
    adapter_bluez->StartDiscoverySession(
        /*client_name=*/std::string(),
        base::BindLambdaForTesting(
            [&loop, this](
                std::unique_ptr<BluetoothDiscoverySession> discovery_session) {
              DiscoverySessionCallback(std::move(discovery_session));
              loop.Quit();
            }),
        GetErrorCallback(loop.QuitClosure()));
  }
  adapter_->Shutdown();
  loop.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(kNumberOfDiscoverySessions, error_callback_count_);
}

TEST_F(BluetoothBlueZTest, StartDiscoveryError_ThenStartAgain) {
  GetAdapter();
  fake_bluetooth_adapter_client_->MakeStartDiscoveryFail();

  {
    base::RunLoop loop;
    adapter_->StartDiscoverySession(
        /*client_name=*/std::string(),
        base::BindOnce([](std::unique_ptr<BluetoothDiscoverySession> session) {
          ADD_FAILURE() << "Unexpected discovery session start success.";
        }),
        loop.QuitClosure());
    loop.Run();
  }

  {
    base::RunLoop loop;
    adapter_->StartDiscoverySession(
        /*client_name=*/std::string(),
        base::BindLambdaForTesting(
            [&](std::unique_ptr<BluetoothDiscoverySession> session) {
              loop.Quit();
            }),
        base::BindOnce([]() {
          ADD_FAILURE() << "Unexpected discovery session start error.";
        }));
    loop.Run();
  }
}

TEST_F(BluetoothBlueZTest, ManufacturerDataChanged) {
  const BluetoothDevice::ManufacturerId kManufacturerId1 = 0x1234;
  const BluetoothDevice::ManufacturerId kManufacturerId2 = 0x2345;
  const BluetoothDevice::ManufacturerId kManufacturerId3 = 0x3456;

  // Simulate a change of manufacturer data of a device.
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);

  // Install an observer; expect the DeviceChanged method to be called
  // when we change the service data.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  properties->manufacturer_data.set_valid(true);

  // Check that ManufacturerDataChanged is correctly invoke.
  properties->manufacturer_data.ReplaceValue({{kManufacturerId1, {1, 2, 3}}});
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());
  EXPECT_EQ(
      BluetoothDevice::ManufacturerDataMap({{kManufacturerId1, {1, 2, 3}}}),
      device->GetManufacturerData());
  EXPECT_EQ(BluetoothDevice::ManufacturerIDSet({kManufacturerId1}),
            device->GetManufacturerDataIDs());
  EXPECT_EQ(std::vector<uint8_t>({1, 2, 3}),
            *(device->GetManufacturerDataForID(kManufacturerId1)));

  // Check that we can update service data with same uuid / add more uuid.
  properties->manufacturer_data.ReplaceValue(
      {{kManufacturerId1, {3, 2, 1}}, {kManufacturerId2, {1}}});
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_EQ(BluetoothDevice::ManufacturerDataMap(
                {{kManufacturerId1, {3, 2, 1}}, {kManufacturerId2, {1}}}),
            device->GetManufacturerData());
  EXPECT_EQ(
      BluetoothDevice::ManufacturerIDSet({kManufacturerId1, kManufacturerId2}),
      device->GetManufacturerDataIDs());
  EXPECT_EQ(std::vector<uint8_t>({3, 2, 1}),
            *(device->GetManufacturerDataForID(kManufacturerId1)));
  EXPECT_EQ(std::vector<uint8_t>({1}),
            *(device->GetManufacturerDataForID(kManufacturerId2)));

  // Check that we can remove uuid / change uuid with same data.
  properties->manufacturer_data.ReplaceValue({{kManufacturerId3, {3, 2, 1}}});
  EXPECT_EQ(3, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_EQ(
      BluetoothDevice::ManufacturerDataMap({{kManufacturerId3, {3, 2, 1}}}),
      device->GetManufacturerData());
  EXPECT_EQ(BluetoothDevice::ManufacturerIDSet({kManufacturerId3}),
            device->GetManufacturerDataIDs());
  EXPECT_EQ(std::vector<uint8_t>({3, 2, 1}),
            *(device->GetManufacturerDataForID(kManufacturerId3)));
  EXPECT_EQ(nullptr, device->GetManufacturerDataForID(kManufacturerId1));
  EXPECT_EQ(nullptr, device->GetManufacturerDataForID(kManufacturerId2));
}

TEST_F(BluetoothBlueZTest, AdvertisingDataFlagsChanged) {
  // Simulate a change of advertising data flags of a device.
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);

  // Install an observer; expect the DeviceChanged method to be called
  // when we change the service data.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  properties->advertising_data_flags.set_valid(true);

  // Check that AdvertisingDataFlagsChanged is correctly invoke.
  properties->advertising_data_flags.ReplaceValue(std::vector<uint8_t>({0x12}));
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());
  EXPECT_TRUE(device->GetAdvertisingDataFlags().has_value());
  EXPECT_EQ(0x12u, device->GetAdvertisingDataFlags().value());

  // Check that we can update advertising data flags.
  properties->advertising_data_flags.ReplaceValue(std::vector<uint8_t>({0x23}));
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());
  EXPECT_TRUE(device->GetAdvertisingDataFlags().has_value());
  EXPECT_EQ(0x23u, device->GetAdvertisingDataFlags().value());
}

TEST_F(BluetoothBlueZTest, SetConnectionLatency) {
  GetAdapter();
  DiscoverDevices();

  // SetConnectionLatency is supported on LE devices.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kConnectUnpairablePath));
  properties->type.ReplaceValue(BluetoothDeviceClient::kTypeLe);
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kConnectUnpairableAddress);
  ASSERT_TRUE(device);

  {
    base::RunLoop loop;

    device->SetConnectionLatency(
        BluetoothDevice::ConnectionLatency::CONNECTION_LATENCY_LOW,
        GetCallback(loop.QuitClosure()), GetErrorCallback(loop.QuitClosure()));
    loop.Run();
  }
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  {
    base::RunLoop loop;

    device->SetConnectionLatency(
        BluetoothDevice::ConnectionLatency::CONNECTION_LATENCY_HIGH,
        GetCallback(loop.QuitClosure()), GetErrorCallback(loop.QuitClosure()));
    loop.Run();
  }
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Dual mode devices should be supported as well.
  properties->type.ReplaceValue(BluetoothDeviceClient::kTypeDual);
  {
    base::RunLoop loop;

    device->SetConnectionLatency(
        BluetoothDevice::ConnectionLatency::CONNECTION_LATENCY_MEDIUM,
        GetCallback(loop.QuitClosure()), GetErrorCallback(loop.QuitClosure()));
    loop.Run();
  }
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // This API is not supported for BR/EDR devices.
  properties->type.ReplaceValue(BluetoothDeviceClient::kTypeBredr);
  {
    base::RunLoop loop;
    device->SetConnectionLatency(
        BluetoothDevice::ConnectionLatency::CONNECTION_LATENCY_MEDIUM,
        GetCallback(loop.QuitClosure()), GetErrorCallback(loop.QuitClosure()));
    loop.Run();
  }
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  // Return an error if the type is not valid.
  properties->type.set_valid(false);
  {
    base::RunLoop loop;
    device->SetConnectionLatency(
        BluetoothDevice::ConnectionLatency::CONNECTION_LATENCY_MEDIUM,
        GetCallback(loop.QuitClosure()), GetErrorCallback(loop.QuitClosure()));
    loop.Run();
  }
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(2, error_callback_count_);
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(BluetoothBlueZTest, AdminPolicyEvents) {
  // Simulate the addition, removal, and change of admin policy.
  GetAdapter();

  // Create a device that will have related admin policy events with.
  dbus::ObjectPath device_path =
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath);
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      device_path);
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  EXPECT_TRUE(device);
  // A new device is not blocked by policy.
  EXPECT_FALSE(device->IsBlockedByPolicy());

  // Create an admin policy, check that the device policy is updated.
  fake_bluetooth_admin_policy_client_->CreateAdminPolicy(
      device_path,
      /*is_blocked_by_policy=*/true);
  EXPECT_TRUE(device->IsBlockedByPolicy());

  // Change the admin policy, check that the device policy is updated.
  fake_bluetooth_admin_policy_client_->ChangeAdminPolicy(
      device_path,
      /*is_blocked_by_policy=*/false);
  EXPECT_FALSE(device->IsBlockedByPolicy());

  // Change the admin policy again, check that the device policy is updated.
  fake_bluetooth_admin_policy_client_->ChangeAdminPolicy(
      device_path,
      /*is_blocked_by_policy=*/true);
  EXPECT_TRUE(device->IsBlockedByPolicy());

  // Remove the admin policy. The device policy should be set back to default
  // (false).
  fake_bluetooth_admin_policy_client_->RemoveAdminPolicy(device_path);
  EXPECT_FALSE(device->IsBlockedByPolicy());
}

TEST_F(BluetoothBlueZTest, AdminPolicyInitBeforeDevice) {
  // Simulate the admin policy being added before the device is added.
  GetAdapter();

  dbus::ObjectPath device_path =
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath);

  // Create an admin policy.
  fake_bluetooth_admin_policy_client_->CreateAdminPolicy(
      device_path,
      /*is_blocked_by_policy=*/true);

  // Create the associated device.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      device_path);
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  EXPECT_TRUE(device);
  // The new device should contain the admin policy.
  EXPECT_TRUE(device->IsBlockedByPolicy());
}
#endif

TEST_F(BluetoothBlueZTest, BatteryEvents) {
  // Simulate the addition, removal, and change of Battery objects.
  GetAdapter();

  // Create a device that will have related battery events with.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  EXPECT_TRUE(device);
  // A new device does not have battery level set.
  EXPECT_FALSE(device->GetBatteryInfo(BatteryType::kDefault).has_value());

  // A battery appears, check that the device battery percentage is updated..
  fake_bluetooth_battery_client_->CreateBattery(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath), 50);
  EXPECT_TRUE(device->GetBatteryInfo(BatteryType::kDefault).has_value());
  EXPECT_EQ(50, device->GetBatteryInfo(BatteryType::kDefault)->percentage);

  // Property percentage changes, check that the corresponding device also
  // updates its battery percentage field.
  fake_bluetooth_battery_client_->ChangeBatteryPercentage(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath), 40);
  EXPECT_EQ(40, device->GetBatteryInfo(BatteryType::kDefault)->percentage);

  // Battery object disappears, check that the corresponding device gets its
  // battery percentage field cleared.
  fake_bluetooth_battery_client_->RemoveBattery(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  EXPECT_FALSE(device->GetBatteryInfo(BatteryType::kDefault).has_value());
}

TEST_F(BluetoothBlueZTest, DeviceUUIDsCombinedFromServiceAndAdvertisement) {
  // Simulate addition and removal of service and advertisement UUIDs.
  GetAdapter();

  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  BluetoothDeviceBlueZ* device_bluez =
      static_cast<BluetoothDeviceBlueZ*>(device);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));

  // Have these 9 UUIDs divided into 3 groups:
  // -UUIDs which is available from the start, but later is missing
  // -UUIDs which is not available in the beginning, but later is added
  // -UUIDs which is always available
  // In each group, they are further divided into 3 groups:
  // -advertisement UUIDs
  // -service UUIDs (from SDP / GATT)
  // -UUIDs which appear in both
  BluetoothUUID uuidInitAdv = BluetoothUUID("1111");
  BluetoothUUID uuidInitServ = BluetoothUUID("2222");
  BluetoothUUID uuidInitBoth = BluetoothUUID("3333");
  BluetoothUUID uuidLaterAdv = BluetoothUUID("4444");
  BluetoothUUID uuidLaterServ = BluetoothUUID("5555");
  BluetoothUUID uuidLaterBoth = BluetoothUUID("6666");
  BluetoothUUID uuidAlwaysAdv = BluetoothUUID("7777");
  BluetoothUUID uuidAlwaysServ = BluetoothUUID("8888");
  BluetoothUUID uuidAlwaysBoth = BluetoothUUID("9999");

  BluetoothAdapter::UUIDList uuidsInitAdv{uuidInitAdv, uuidInitBoth,
                                          uuidAlwaysAdv, uuidAlwaysBoth};
  BluetoothAdapter::UUIDList uuidsLaterAdv{uuidLaterAdv, uuidLaterBoth,
                                           uuidAlwaysAdv, uuidAlwaysBoth};

  std::vector<std::string> uuidsInitServ{
      uuidInitServ.canonical_value(), uuidInitBoth.canonical_value(),
      uuidAlwaysServ.canonical_value(), uuidAlwaysBoth.canonical_value()};
  std::vector<std::string> uuidsLaterServ{
      uuidLaterServ.canonical_value(), uuidLaterBoth.canonical_value(),
      uuidAlwaysServ.canonical_value(), uuidAlwaysBoth.canonical_value()};

  device_bluez->SetAdvertisedUUIDs(uuidsInitAdv);
  properties->uuids.ReplaceValue(uuidsInitServ);

  // The result should be the union of service and advertisement UUIDs
  BluetoothDevice::UUIDSet dev_uuids = device->GetUUIDs();
  EXPECT_THAT(dev_uuids, ::testing::ElementsAre(
                             uuidInitAdv, uuidInitServ, uuidInitBoth,
                             uuidAlwaysAdv, uuidAlwaysServ, uuidAlwaysBoth));

  // advertisement UUIDs updated
  device_bluez->SetAdvertisedUUIDs(uuidsLaterAdv);
  dev_uuids = device->GetUUIDs();
  EXPECT_THAT(dev_uuids,
              ::testing::ElementsAre(uuidInitServ, uuidInitBoth, uuidLaterAdv,
                                     uuidLaterBoth, uuidAlwaysAdv,
                                     uuidAlwaysServ, uuidAlwaysBoth));

  // service UUIDs updated
  properties->uuids.ReplaceValue(uuidsLaterServ);
  dev_uuids = device->GetUUIDs();
  EXPECT_THAT(dev_uuids, ::testing::ElementsAre(
                             uuidLaterAdv, uuidLaterServ, uuidLaterBoth,
                             uuidAlwaysAdv, uuidAlwaysServ, uuidAlwaysBoth));
}
#if BUILDFLAG(IS_CHROMEOS)
TEST_F(BluetoothBlueZTest, StartLowEnergyScanSessionAdapterPresent) {
  GetAdapter();

  FakeBluetoothAdvertisementMonitorApplicationServiceProvider*
      application_manager = GetAdvertisementMonitorApplicationManger();

  auto filter = CreateLowEnergyScanFilter();
  FakeBluetoothLowEnergyScanSessionDelegate delegate;

  auto background_scan_session = adapter_->StartLowEnergyScanSession(
      std::move(filter), /*delegate=*/delegate.GetWeakPtr());

  // Check that advertisement monitor was added to d-bus layer.
  ASSERT_EQ(1u, application_manager->AdvertisementMonitorsCount());

  // Check that advertisement monitor gets removed from d-bus layer when the
  // scan session is destroyed.
  background_scan_session.reset();
  ASSERT_EQ(0u, application_manager->AdvertisementMonitorsCount());
}

TEST_F(BluetoothBlueZTest, StartLowEnergyScanSessionAdapterAddedLater) {
  fake_bluetooth_adapter_client_->SetPresent(false);
  GetAdapter();
  ASSERT_FALSE(adapter_->IsPresent());

  FakeBluetoothAdvertisementMonitorApplicationServiceProvider*
      application_manager = GetAdvertisementMonitorApplicationManger();

  auto filter = CreateLowEnergyScanFilter();

  FakeBluetoothLowEnergyScanSessionDelegate delegate;
  auto background_scan_session = adapter_->StartLowEnergyScanSession(
      std::move(filter), /*delegate=*/delegate.GetWeakPtr());

  // Check that the advertisement monitor is not yet added to the d-bus layer.
  // It is queued up until the adapter gets added.
  ASSERT_EQ(0u, application_manager->AdvertisementMonitorsCount());

  fake_bluetooth_adapter_client_->SetPresent(true);
  EXPECT_TRUE(adapter_->IsPresent());

  ASSERT_EQ(1u, application_manager->AdvertisementMonitorsCount());

  background_scan_session.reset();
  ASSERT_EQ(0u, application_manager->AdvertisementMonitorsCount());
}

TEST_F(BluetoothBlueZTest, StartLowEnergyScanSessionAdapterBecomeNotPresent) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  // Remove Adapter
  fake_bluetooth_adapter_client_->SetPresent(false);

  FakeBluetoothAdvertisementMonitorApplicationServiceProvider*
      application_manager = GetAdvertisementMonitorApplicationManger();

  auto filter = CreateLowEnergyScanFilter();
  FakeBluetoothLowEnergyScanSessionDelegate delegate;

  auto background_scan_session = adapter_->StartLowEnergyScanSession(
      std::move(filter), /*delegate=*/delegate.GetWeakPtr());

  // Check that advertisement monitor was not added to d-bus layer since there
  // is no adapter present.
  ASSERT_EQ(0u, application_manager->AdvertisementMonitorsCount());

  // Add Adapter
  fake_bluetooth_adapter_client_->SetPresent(true);

  // Check that queued advertisement monitor was added to d-bus after adapter
  // becomes present.
  ASSERT_EQ(1u, application_manager->AdvertisementMonitorsCount());
}

TEST_F(BluetoothBlueZTest, BluetoothLowEnergyScanSessionBlueZDeviceFound) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  FakeBluetoothAdvertisementMonitorApplicationServiceProvider*
      application_manager = GetAdvertisementMonitorApplicationManger();
  FakeBluetoothLowEnergyScanSessionDelegate delegate;
  auto background_scan_session = adapter_->StartLowEnergyScanSession(
      CreateLowEnergyScanFilter(), /*delegate=*/delegate.GetWeakPtr());

  // Check that advertisement monitor was added to d-bus layer.
  EXPECT_EQ(1u, application_manager->AdvertisementMonitorsCount());

  // Get advertisement fake advertisement monitor to forward events to
  // BluetoothLowEnergyScanSessionBlueZ.
  FakeBluetoothAdvertisementMonitorServiceProvider* advertisement_monitor =
      application_manager->GetLastAddedAdvertisementMonitorServiceProvider();
  ASSERT_TRUE(advertisement_monitor);

  // Simulate a device found event.
  advertisement_monitor->delegate()->OnDeviceFound(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kPairedDevicePath));
  EXPECT_EQ(1u, delegate.devices_found().size());

  std::pair<device::BluetoothLowEnergyScanSession*, device::BluetoothDevice*>
      devices_found_pair = delegate.devices_found()[0];
  EXPECT_EQ(background_scan_session.get(), devices_found_pair.first);
  EXPECT_EQ(adapter_->GetDevice(
                bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress),
            devices_found_pair.second);
}

TEST_F(BluetoothBlueZTest, BluetoothLowEnergyScanSessionBlueZDeviceNULL) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  FakeBluetoothAdvertisementMonitorApplicationServiceProvider*
      application_manager = GetAdvertisementMonitorApplicationManger();
  FakeBluetoothLowEnergyScanSessionDelegate delegate;
  auto background_scan_session = adapter_->StartLowEnergyScanSession(
      CreateLowEnergyScanFilter(), /*delegate=*/delegate.GetWeakPtr());

  // Check that advertisement monitor was added to d-bus layer.
  EXPECT_EQ(1u, application_manager->AdvertisementMonitorsCount());

  // Get advertisement fake advertisement monitor to forward events to
  // BluetoothLowEnergyScanSessionBlueZ.
  FakeBluetoothAdvertisementMonitorServiceProvider* advertisement_monitor =
      application_manager->GetLastAddedAdvertisementMonitorServiceProvider();
  ASSERT_TRUE(advertisement_monitor);

  bool did_dump_without_crashing = false;
  dump_without_crashing_flag = &did_dump_without_crashing;
  base::debug::SetDumpWithoutCrashingFunction(&HandleDumpWithoutCrashing);
  advertisement_monitor->delegate()->OnDeviceFound(dbus::ObjectPath(""));

  EXPECT_TRUE(did_dump_without_crashing);

  base::debug::SetDumpWithoutCrashingFunction(nullptr);
}

TEST_F(BluetoothBlueZTest, BluetoothLowEnergyScanSessionBlueZDeviceLost) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  FakeBluetoothAdvertisementMonitorApplicationServiceProvider*
      application_manager = GetAdvertisementMonitorApplicationManger();
  FakeBluetoothLowEnergyScanSessionDelegate delegate;
  auto background_scan_session = adapter_->StartLowEnergyScanSession(
      CreateLowEnergyScanFilter(), /*delegate=*/delegate.GetWeakPtr());

  // Check that advertisement monitor was added to d-bus layer.
  EXPECT_EQ(1u, application_manager->AdvertisementMonitorsCount());

  // Get advertisement fake advertisement monitor to forward events to
  // BluetoothLowEnergyScanSessionBlueZ.
  FakeBluetoothAdvertisementMonitorServiceProvider* advertisement_monitor =
      application_manager->GetLastAddedAdvertisementMonitorServiceProvider();
  ASSERT_TRUE(advertisement_monitor);

  // Simulate a device lost event.
  advertisement_monitor->delegate()->OnDeviceLost(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kPairedDevicePath));
  EXPECT_EQ(1u, delegate.devices_lost().size());

  std::pair<device::BluetoothLowEnergyScanSession*, device::BluetoothDevice*>
      devices_lost_pair = delegate.devices_lost()[0];
  EXPECT_EQ(background_scan_session.get(), devices_lost_pair.first);
  EXPECT_EQ(adapter_->GetDevice(
                bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress),
            devices_lost_pair.second);
}

TEST_F(BluetoothBlueZTest,
       BluetoothLowEnergyScanSessionBlueZStartThenInvalidate) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  FakeBluetoothAdvertisementMonitorApplicationServiceProvider*
      application_manager = GetAdvertisementMonitorApplicationManger();
  FakeBluetoothLowEnergyScanSessionDelegate delegate;
  auto background_scan_session = adapter_->StartLowEnergyScanSession(
      CreateLowEnergyScanFilter(), /*delegate=*/delegate.GetWeakPtr());

  // Check that advertisement monitor was added to d-bus layer.
  EXPECT_EQ(1u, application_manager->AdvertisementMonitorsCount());

  // Get advertisement fake advertisement monitor to forward events to
  // BluetoothLowEnergyScanSessionBlueZ.
  FakeBluetoothAdvertisementMonitorServiceProvider* advertisement_monitor =
      application_manager->GetLastAddedAdvertisementMonitorServiceProvider();
  ASSERT_TRUE(advertisement_monitor);

  // Successfully start scan session.
  advertisement_monitor->delegate()->OnActivate();
  EXPECT_EQ(1u, delegate.sessions_started().size());
  std::pair<device::BluetoothLowEnergyScanSession*,
            std::optional<device::BluetoothLowEnergyScanSession::ErrorCode>>
      session_started_pair = delegate.sessions_started()[0];

  // Check that the correct scan session is started.
  EXPECT_EQ(background_scan_session.get(), session_started_pair.first);

  // Check that there was no error when starting the scan session.
  EXPECT_FALSE(session_started_pair.second.has_value());

  // Invalidate scan session after successful start.
  advertisement_monitor->delegate()->OnRelease();
  EXPECT_EQ(1u, delegate.sessions_invalidated().size());
  EXPECT_EQ(background_scan_session.get(),
            delegate.sessions_invalidated().front());
}

TEST_F(BluetoothBlueZTest, BluetoothLowEnergyScanSessionBlueZFailsToStart) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  FakeBluetoothAdvertisementMonitorApplicationServiceProvider*
      application_manager = GetAdvertisementMonitorApplicationManger();
  FakeBluetoothLowEnergyScanSessionDelegate delegate;
  auto background_scan_session = adapter_->StartLowEnergyScanSession(
      CreateLowEnergyScanFilter(), /*delegate=*/delegate.GetWeakPtr());

  // Check that advertisement monitor was added to d-bus layer.
  EXPECT_EQ(1u, application_manager->AdvertisementMonitorsCount());

  // Get advertisement fake advertisement monitor to forward events to
  // BluetoothLowEnergyScanSessionBlueZ.
  FakeBluetoothAdvertisementMonitorServiceProvider* advertisement_monitor =
      application_manager->GetLastAddedAdvertisementMonitorServiceProvider();
  ASSERT_TRUE(advertisement_monitor);

  // Scan session failed to start.
  advertisement_monitor->delegate()->OnRelease();
  EXPECT_EQ(1u, delegate.sessions_started().size());

  std::pair<device::BluetoothLowEnergyScanSession*,
            std::optional<device::BluetoothLowEnergyScanSession::ErrorCode>>
      session_started_pair = delegate.sessions_started()[0];

  // Check that the correct scan session.
  EXPECT_EQ(background_scan_session.get(), session_started_pair.first);

  // Check that there was an error indicating failure to start.
  EXPECT_TRUE(session_started_pair.second.has_value());
}

TEST_F(BluetoothBlueZTest,
       LowEnergyScanSession_HardwareOffloadingNotSupported) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  // We haven't added any supported features to the list, so this should report
  // hardware offloading as not supported.
  EXPECT_EQ(adapter_->GetLowEnergyScanSessionHardwareOffloadingStatus(),
            BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus::
                kNotSupported);
}

TEST_F(BluetoothBlueZTest, LowEnergyScanSession_HardwareOffloadingSupport) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  // Install an observer;
  TestBluetoothAdapterObserver observer(adapter_);

  BluetoothAdapterBlueZ* adapter_bluez =
      static_cast<BluetoothAdapterBlueZ*>(adapter_.get());
  FakeBluetoothAdvertisementMonitorManagerClient* client =
      static_cast<bluez::FakeBluetoothAdvertisementMonitorManagerClient*>(
          bluez::BluezDBusManager::Get()
              ->GetBluetoothAdvertisementMonitorManagerClient());

  // If no properties are returned we should get |kUndetermined| status.
  client->RemoveProperties();
  EXPECT_EQ(adapter_->GetLowEnergyScanSessionHardwareOffloadingStatus(),
            BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus::
                kUndetermined);
  EXPECT_EQ(observer.last_low_energy_scan_session_hardware_offloading_status(),
            BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus::
                kUndetermined);

  // Once we add the "controller-patterns" feature, the adapter should report
  // hardware offloading as supported.
  client->InitializeProperties();
  BluetoothAdvertisementMonitorManagerClient::Properties* properties =
      client->GetProperties(adapter_bluez->object_path());
  ASSERT_TRUE(properties);
  properties->supported_features.ReplaceValue(
      {bluetooth_advertisement_monitor_manager::
           kSupportedFeaturesControllerPatterns});
  EXPECT_EQ(adapter_->GetLowEnergyScanSessionHardwareOffloadingStatus(),
            BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus::
                kSupported);
  EXPECT_EQ(observer.last_low_energy_scan_session_hardware_offloading_status(),
            BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus::
                kSupported);

  properties->supported_features.ReplaceValue({});
  EXPECT_EQ(adapter_->GetLowEnergyScanSessionHardwareOffloadingStatus(),
            BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus::
                kNotSupported);
  EXPECT_EQ(observer.last_low_energy_scan_session_hardware_offloading_status(),
            BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus::
                kNotSupported);

  // Ensure that if no properties are returned we get the |kUndetermined|
  // status.
  client->RemoveProperties();
  EXPECT_EQ(adapter_->GetLowEnergyScanSessionHardwareOffloadingStatus(),
            BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus::
                kUndetermined);
}

TEST_F(BluetoothBlueZTest, IsExtendedAdvertisementsAvailable) {
  GetAdapter();

  BluetoothAdapterBlueZ* adapter_bluez =
      static_cast<BluetoothAdapterBlueZ*>(adapter_.get());

  FakeBluetoothLEAdvertisingManagerClient* client =
      static_cast<bluez::FakeBluetoothLEAdvertisingManagerClient*>(
          bluez::BluezDBusManager::Get()
              ->GetBluetoothLEAdvertisingManagerClient());

  BluetoothLEAdvertisingManagerClient::Properties* properties =
      client->GetProperties(adapter_bluez->object_path());
  ASSERT_TRUE(properties);

  std::vector<std::string> supported_features = {};

  properties->supported_features.ReplaceValue(supported_features);

  // Empty supported feature indicates the adapter doesn't support Ext
  // Advertising
  EXPECT_FALSE(adapter_bluez->IsExtendedAdvertisementsAvailable());

  supported_features.push_back(std::string(
      bluetooth_advertising_manager::kSupportedFeaturesHardwareOffload));

  // HardwareOffload indicates the adapter support Ext Advertising
  properties->supported_features.ReplaceValue(supported_features);

  EXPECT_TRUE(adapter_bluez->IsExtendedAdvertisementsAvailable());
}

TEST_F(BluetoothBlueZTest, GetSupportedRoles) {
  std::vector<std::string> adapter_roles;
  GetAdapter();

  ASSERT_TRUE(adapter_->GetSupportedRoles().empty());

  // An unknown role should be ignored
  adapter_roles.push_back("unknown-role");
  fake_bluetooth_adapter_client_->SetRoles(adapter_roles);
  ASSERT_TRUE(adapter_->GetSupportedRoles().empty());

  adapter_roles.push_back("central");
  fake_bluetooth_adapter_client_->SetRoles(adapter_roles);
  EXPECT_EQ(1u, adapter_->GetSupportedRoles().size());
  ASSERT_TRUE(base::Contains(adapter_->GetSupportedRoles(),
                             BluetoothAdapter::BluetoothRole::kCentral));

  adapter_roles.push_back("peripheral");
  fake_bluetooth_adapter_client_->SetRoles(adapter_roles);
  EXPECT_EQ(2u, adapter_->GetSupportedRoles().size());
  ASSERT_TRUE(base::Contains(adapter_->GetSupportedRoles(),
                             BluetoothAdapter::BluetoothRole::kPeripheral));

  adapter_roles.push_back("central-peripheral");
  fake_bluetooth_adapter_client_->SetRoles(adapter_roles);
  EXPECT_EQ(3u, adapter_->GetSupportedRoles().size());
  ASSERT_TRUE(
      base::Contains(adapter_->GetSupportedRoles(),
                     BluetoothAdapter::BluetoothRole::kCentralPeripheral));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace bluez
