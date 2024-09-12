// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/web_bluetooth_service_impl.h"

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/browser/bluetooth/bluetooth_adapter_factory_wrapper.h"
#include "content/browser/bluetooth/bluetooth_allowed_devices.h"
#include "content/browser/bluetooth/web_bluetooth_pairing_manager.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_notify_session.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

namespace content {

namespace {

using ::base::test::TestFuture;
using ::blink::mojom::WebBluetoothCharacteristicClient;
using ::blink::mojom::WebBluetoothGATTQueryQuantity;
using ::blink::mojom::WebBluetoothRemoteGATTCharacteristicPtr;
using ::blink::mojom::WebBluetoothRemoteGATTServicePtr;
using ::blink::mojom::WebBluetoothResult;
using ::device::BluetoothDevice;
using ::device::BluetoothGattService;
using ::device::BluetoothRemoteGattCharacteristic;
using ::device::BluetoothRemoteGattService;
using ::device::BluetoothUUID;
using ::device::MockBluetoothAdapter;
using ::device::MockBluetoothDevice;
using ::device::MockBluetoothGattCharacteristic;
using ::device::MockBluetoothGattNotifySession;
using ::device::MockBluetoothGattService;
using ::testing::_;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::WithParamInterface;

using PromptEventCallback =
    base::OnceCallback<void(BluetoothScanningPrompt::Event)>;
using WatchAdvertisementsForDeviceCallback =
    base::OnceCallback<void(WebBluetoothResult)>;

// Plain data struct for fake bluetooth device.
struct BluetoothDeviceBundleData {
  std::string_view service_id;
  std::string_view chracteristic_id;
  device::BluetoothUUID service_uuid;
  device::BluetoothUUID chracteristic_uuid;
  BluetoothRemoteGattCharacteristic::Properties characteristic_properties;
};

constexpr BluetoothRemoteGattCharacteristic::Properties
    kTestCharacteristicProperties =
        BluetoothRemoteGattCharacteristic::PROPERTY_BROADCAST |
        BluetoothRemoteGattCharacteristic::PROPERTY_READ |
        BluetoothRemoteGattCharacteristic::PROPERTY_INDICATE;

// Constants for battery service fake bluetooth device.
const char kBatteryServiceUUIDString[] = "0000180f-0000-1000-8000-00805f9b34fb";
constexpr char kBatteryServiceId[] = "battery_service_id";
constexpr char kBatteryLevelCharacteristicId[] = "battery_level_id";
const device::BluetoothUUID kBatteryServiceUUID(kBatteryServiceUUIDString);
const device::BluetoothUUID kBatteryLevelCharacteristicUUID(
    "00002a19-0000-1000-8000-00805f9b34fb");
const BluetoothDeviceBundleData battery_device_bundle_data = {
    kBatteryServiceId, kBatteryLevelCharacteristicId, kBatteryServiceUUID,
    kBatteryLevelCharacteristicUUID, kTestCharacteristicProperties};

// Constants for heart rate service fake bluetooth device.
const char kHeartRateServiceUUIDString[] =
    "0000180d-0000-1000-8000-00805f9b34fb";
constexpr char kHeartRateServiceId[] = "heart_rate_service_id";
constexpr char kHeartRateMeasurementCharacteristicId[] =
    "heart_rate_measurement_id";
const device::BluetoothUUID kHeartRateServiceUUID(kHeartRateServiceUUIDString);
const device::BluetoothUUID kHeartRateMeasurementCharacteristicUUID(
    "00002a37-0000-1000-8000-00805f9b34fb");
const BluetoothDeviceBundleData heart_rate_device_bundle_data = {
    kHeartRateServiceId, kHeartRateMeasurementCharacteristicId,
    kHeartRateServiceUUID, kHeartRateMeasurementCharacteristicUUID,
    kTestCharacteristicProperties};

class MockWebBluetoothPairingManager : public WebBluetoothPairingManager {
 public:
  MockWebBluetoothPairingManager() = default;
  MockWebBluetoothPairingManager(const MockWebBluetoothPairingManager&) =
      delete;
  MockWebBluetoothPairingManager& operator=(
      const MockWebBluetoothPairingManager&) = delete;
  ~MockWebBluetoothPairingManager() override = default;

  MOCK_METHOD2(PairForCharacteristicReadValue,
               void(const std::string& characteristic_instance_id,
                    blink::mojom::WebBluetoothService::
                        RemoteCharacteristicReadValueCallback read_callback));
  MOCK_METHOD4(PairForCharacteristicWriteValue,
               void(const std::string& characteristic_instance_id,
                    const std::vector<uint8_t>& value,
                    blink::mojom::WebBluetoothWriteType write_type,
                    blink::mojom::WebBluetoothService::
                        RemoteCharacteristicWriteValueCallback callback));
  MOCK_METHOD2(
      PairForDescriptorReadValue,
      void(const std::string& descriptor_instance_id,
           blink::mojom::WebBluetoothService::RemoteDescriptorReadValueCallback
               read_callback));
  MOCK_METHOD3(
      PairForDescriptorWriteValue,
      void(const std::string& descriptor_instance_id,
           const std::vector<uint8_t>& value,
           blink::mojom::WebBluetoothService::RemoteDescriptorWriteValueCallback
               callback));
  MOCK_METHOD3(
      PairForCharacteristicStartNotifications,
      void(const std::string& characteristic_instance_id,
           mojo::AssociatedRemote<
               blink::mojom::WebBluetoothCharacteristicClient> client,
           blink::mojom::WebBluetoothService::
               RemoteCharacteristicStartNotificationsCallback callback));
};

class FakeBluetoothScanningPrompt : public BluetoothScanningPrompt {
 public:
  explicit FakeBluetoothScanningPrompt(
      PromptEventCallback prompt_event_callback)
      : prompt_event_callback_(std::move(prompt_event_callback)) {}
  ~FakeBluetoothScanningPrompt() override = default;

  // Move-only class.
  FakeBluetoothScanningPrompt(const FakeBluetoothScanningPrompt&) = delete;
  FakeBluetoothScanningPrompt& operator=(const FakeBluetoothScanningPrompt&) =
      delete;

  void RunPromptEventCallback(Event event) {
    if (prompt_event_callback_.is_null()) {
      FAIL() << "prompt_event_callback_ is not set";
    }
    std::move(prompt_event_callback_).Run(event);
  }

 private:
  PromptEventCallback prompt_event_callback_;
};

class FakeWebBluetoothCharacteristicClient : WebBluetoothCharacteristicClient {
 public:
  mojo::PendingAssociatedRemote<WebBluetoothCharacteristicClient>
  BindNewEndpointClientAndPassRemote() {
    receiver_.reset();
    return receiver_.BindNewEndpointAndPassDedicatedRemote();
  }

 protected:
  // WebBluetoothCharacteristicClient implementation:
  void RemoteCharacteristicValueChanged(
      const std::vector<uint8_t>& value) override {
    NOTREACHED_IN_MIGRATION();
  }

 private:
  mojo::AssociatedReceiver<WebBluetoothCharacteristicClient> receiver_{this};
};

class FakeBluetoothAdapter : public device::MockBluetoothAdapter {
 public:
  FakeBluetoothAdapter() = default;

  // Move-only class.
  FakeBluetoothAdapter(const FakeBluetoothAdapter&) = delete;
  FakeBluetoothAdapter& operator=(const FakeBluetoothAdapter&) = delete;

  // device::BluetoothAdapter:
  void StartScanWithFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override {
    // PostTask here to simulate fake adapter return start discovery result
    // asynchronously.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), is_start_scan_result_error_,
                       start_scan_result_));
  }

  void AddObserver(BluetoothAdapter::Observer* observer) override {}

  void RemoveObserver(BluetoothAdapter::Observer* observer) override {}

  BluetoothDevice* GetDevice(const std::string& address) override {
    for (auto& device : mock_devices_) {
      if (device->GetAddress() == address) {
        return device.get();
      }
    }
    return nullptr;
  }

  void SetStartScanWithFilterResult(
      device::UMABluetoothDiscoverySessionOutcome result) {
    start_scan_result_ = result;
    is_start_scan_result_error_ =
        result == device::UMABluetoothDiscoverySessionOutcome::SUCCESS ? false
                                                                       : true;
  }

 private:
  ~FakeBluetoothAdapter() override = default;

  // Inputs for `DiscoverySessionResultCallback`.
  bool is_start_scan_result_error_;
  device::UMABluetoothDiscoverySessionOutcome start_scan_result_;
};

class FakeBluetoothGattService : public NiceMock<MockBluetoothGattService> {
 public:
  FakeBluetoothGattService(MockBluetoothDevice* device,
                           const std::string& identifier,
                           const device::BluetoothUUID& uuid)
      : NiceMock<MockBluetoothGattService>(device,
                                           identifier,
                                           uuid,
                                           /*is_primary=*/true) {}
};

class FakeBluetoothDevice : public NiceMock<MockBluetoothDevice> {
 public:
  explicit FakeBluetoothDevice(MockBluetoothAdapter* adapter)
      : NiceMock<MockBluetoothDevice>(adapter,
                                      /*bluetooth_class=*/0,
                                      /*name=*/"device with battery",
                                      /*address=*/"00:00:01",
                                      /*paired=*/false,
                                      /*connected=*/true) {}

  bool IsGattServicesDiscoveryComplete() const override {
    return gatt_services_discovery_complete_;
  }

  std::vector<BluetoothRemoteGattService*> GetGattServices() const override {
    return GetMockServices();
  }

  BluetoothRemoteGattService* GetGattService(
      const std::string& identifier) const override {
    return GetMockService(identifier);
  }
};

class FakeBluetoothCharacteristic
    : public NiceMock<MockBluetoothGattCharacteristic> {
 public:
  FakeBluetoothCharacteristic(MockBluetoothGattService* service,
                              const std::string& identifier,
                              const device::BluetoothUUID& uuid,
                              Properties properties,
                              Permissions permissions)
      : NiceMock<MockBluetoothGattCharacteristic>(service,
                                                  identifier,
                                                  uuid,
                                                  properties,
                                                  permissions) {}

  void StartNotifySession(NotifySessionCallback callback,
                          ErrorCallback error_callback) override {
    if (defer_next_start_notification_) {
      defer_next_start_notification_ = false;
      DCHECK(deferred_start_notification_callback_.is_null());
      DCHECK(deferred_start_notification_error_callback_.is_null());
      deferred_start_notification_callback_ = std::move(callback);
      deferred_start_notification_error_callback_ = std::move(error_callback);
      return;
    }

    std::move(callback).Run(
        std::make_unique<MockBluetoothGattNotifySession>(GetWeakPtr()));
  }

  void ResumeDeferredStartNotification() {
    if (notification_start_error_code_.has_value()) {
      deferred_start_notification_callback_.Reset();
      std::move(deferred_start_notification_error_callback_)
          .Run(notification_start_error_code_.value());
    } else {
      deferred_start_notification_error_callback_.Reset();
      std::move(deferred_start_notification_callback_)
          .Run(std::make_unique<MockBluetoothGattNotifySession>(GetWeakPtr()));
    }
  }

  void DeferNextStartNotification(
      std::optional<BluetoothGattService::GattErrorCode> error_code) {
    defer_next_start_notification_ = true;
    notification_start_error_code_ = error_code;
  }

 private:
  bool defer_next_start_notification_ = false;
  std::optional<BluetoothGattService::GattErrorCode>
      notification_start_error_code_;
  NotifySessionCallback deferred_start_notification_callback_;
  ErrorCallback deferred_start_notification_error_callback_;
};

class TestBluetoothDelegate : public BluetoothDelegate {
 public:
  TestBluetoothDelegate() = default;
  ~TestBluetoothDelegate() override = default;
  TestBluetoothDelegate(const TestBluetoothDelegate&) = delete;
  TestBluetoothDelegate& operator=(const TestBluetoothDelegate&) = delete;

  // BluetoothDelegate:
  std::unique_ptr<BluetoothChooser> RunBluetoothChooser(
      RenderFrameHost* frame,
      const BluetoothChooser::EventHandler& event_handler) override {
    return nullptr;
  }
  std::unique_ptr<BluetoothScanningPrompt> ShowBluetoothScanningPrompt(
      RenderFrameHost* frame,
      const BluetoothScanningPrompt::EventHandler& event_handler) override {
    auto prompt =
        std::make_unique<FakeBluetoothScanningPrompt>(std::move(event_handler));
    prompt_ = prompt.get();
    return std::move(prompt);
  }

  void ShowDevicePairPrompt(content::RenderFrameHost* frame,
                            const std::u16string& device_identifier,
                            PairPromptCallback callback,
                            PairingKind pairing_kind,
                            const std::optional<std::u16string>& pin) override {
    std::move(callback).Run(PairPromptResult(PairPromptStatus::kCancelled));
  }

  blink::WebBluetoothDeviceId GetWebBluetoothDeviceId(
      RenderFrameHost* frame,
      const std::string& device_address) override {
    return blink::WebBluetoothDeviceId();
  }
  std::string GetDeviceAddress(RenderFrameHost* frame,
                               const blink::WebBluetoothDeviceId&) override {
    return std::string();
  }
  blink::WebBluetoothDeviceId AddScannedDevice(
      RenderFrameHost* frame,
      const std::string& device_address) override {
    return blink::WebBluetoothDeviceId();
  }
  blink::WebBluetoothDeviceId GrantServiceAccessPermission(
      RenderFrameHost* frame,
      const device::BluetoothDevice* device,
      const blink::mojom::WebBluetoothRequestDeviceOptions* options) override {
    return blink::WebBluetoothDeviceId();
  }
  bool HasDevicePermission(
      RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override {
    return false;
  }
  void RevokeDevicePermissionWebInitiated(
      RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override {}
  bool MayUseBluetooth(RenderFrameHost* rfh) override { return true; }
  bool IsAllowedToAccessService(RenderFrameHost* frame,
                                const blink::WebBluetoothDeviceId& device_id,
                                const device::BluetoothUUID& service) override {
    return false;
  }
  bool IsAllowedToAccessAtLeastOneService(
      RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override {
    return false;
  }
  bool IsAllowedToAccessManufacturerData(
      RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id,
      const uint16_t manufacturer_code) override {
    return false;
  }
  std::vector<blink::mojom::WebBluetoothDevicePtr> GetPermittedDevices(
      RenderFrameHost* frame) override {
    return {};
  }

  void RunBluetoothScanningPromptEventCallback(
      BluetoothScanningPrompt::Event event) {
    if (!prompt_) {
      FAIL() << "ShowBluetoothScanningPrompt must be called before "
             << __func__;
    }
    prompt_->RunPromptEventCallback(event);
  }

  void AddFramePermissionObserver(FramePermissionObserver* observer) override {}
  void RemoveFramePermissionObserver(
      FramePermissionObserver* observer) override {}

 private:
  raw_ptr<FakeBluetoothScanningPrompt, AcrossTasksDanglingUntriaged> prompt_ =
      nullptr;
};

class TestContentBrowserClient : public ContentBrowserClient {
 public:
  TestContentBrowserClient() = default;
  ~TestContentBrowserClient() override = default;
  TestContentBrowserClient(const TestContentBrowserClient&) = delete;
  TestContentBrowserClient& operator=(const TestContentBrowserClient&) = delete;

  TestBluetoothDelegate* bluetooth_delegate() { return &bluetooth_delegate_; }

 protected:
  // ChromeContentBrowserClient:
  BluetoothDelegate* GetBluetoothDelegate() override {
    return &bluetooth_delegate_;
  }

 private:
  TestBluetoothDelegate bluetooth_delegate_;
};

class FakeWebBluetoothAdvertisementClient
    : blink::mojom::WebBluetoothAdvertisementClient {
 public:
  FakeWebBluetoothAdvertisementClient() = default;
  ~FakeWebBluetoothAdvertisementClient() override = default;

  // Move-only class.
  FakeWebBluetoothAdvertisementClient(
      const FakeWebBluetoothAdvertisementClient&) = delete;
  FakeWebBluetoothAdvertisementClient& operator=(
      const FakeWebBluetoothAdvertisementClient&) = delete;

  // blink::mojom::WebBluetoothAdvertisementClient:
  void AdvertisingEvent(
      blink::mojom::WebBluetoothAdvertisingEventPtr event) override {}

  void BindReceiver(mojo::PendingAssociatedReceiver<
                    blink::mojom::WebBluetoothAdvertisementClient> receiver) {
    receiver_.Bind(std::move(receiver));
    receiver_.set_disconnect_handler(
        base::BindOnce(&FakeWebBluetoothAdvertisementClient::OnConnectionError,
                       base::Unretained(this)));
  }

  void OnConnectionError() { on_connection_error_called_ = true; }

  bool on_connection_error_called() { return on_connection_error_called_; }

 private:
  mojo::AssociatedReceiver<blink::mojom::WebBluetoothAdvertisementClient>
      receiver_{this};
  bool on_connection_error_called_ = false;
};

// A collection of Bluetooth objects which present related
// device/service/characteristic/AdvertisementClient instances for
// device level testing.
class FakeBluetoothDeviceBundle {
 public:
  explicit FakeBluetoothDeviceBundle(
      scoped_refptr<FakeBluetoothAdapter> adapter,
      BluetoothDeviceBundleData bundle_data)
      : adapter_(std::move(adapter)) {
    auto device = std::make_unique<FakeBluetoothDevice>(adapter_.get());
    device_ = device.get();

    auto service = std::make_unique<FakeBluetoothGattService>(
        device.get(), bundle_data.service_id.data(), bundle_data.service_uuid);
    service_ = service.get();

    auto characteristic = std::make_unique<FakeBluetoothCharacteristic>(
        service_, bundle_data.chracteristic_id.data(),
        bundle_data.chracteristic_uuid, bundle_data.characteristic_properties,
        BluetoothRemoteGattCharacteristic::PERMISSION_NONE);
    characteristic_ = characteristic.get();
    service->AddMockCharacteristic(std::move(characteristic));
    device->AddMockService(std::move(service));
    adapter_->AddMockDevice(std::move(device));
    advertisement_client_ =
        std::make_unique<FakeWebBluetoothAdvertisementClient>();
  }

  FakeBluetoothDeviceBundle& operator=(const FakeBluetoothDeviceBundle&) =
      delete;

  FakeBluetoothDevice& device() { return *device_; }

  FakeBluetoothGattService& service() { return *service_; }

  FakeBluetoothCharacteristic& characteristic() { return *characteristic_; }

  FakeWebBluetoothAdvertisementClient& advertisement_client() {
    return *advertisement_client_;
  }

 private:
  scoped_refptr<FakeBluetoothAdapter> adapter_;
  raw_ptr<FakeBluetoothDevice> device_ = nullptr;
  raw_ptr<FakeBluetoothGattService> service_ = nullptr;
  raw_ptr<FakeBluetoothCharacteristic> characteristic_ = nullptr;
  std::unique_ptr<FakeWebBluetoothAdvertisementClient> advertisement_client_;
};

}  // namespace

class WebBluetoothServiceImplTest : public RenderViewHostImplTestHarness,
                                    public WithParamInterface<bool> {
 public:
  WebBluetoothServiceImplTest() = default;
  ~WebBluetoothServiceImplTest() override = default;

  // Move-only class.
  WebBluetoothServiceImplTest(const WebBluetoothServiceImplTest&) = delete;
  WebBluetoothServiceImplTest& operator=(const WebBluetoothServiceImplTest&) =
      delete;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    // Set up an adapter.
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    EXPECT_CALL(*adapter_, IsPresent()).WillRepeatedly(Return(true));
    BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterOverride(adapter_);
    battery_device_bundle_ = std::make_unique<FakeBluetoothDeviceBundle>(
        adapter_, battery_device_bundle_data);

    heart_rate_device_bundle_ = std::make_unique<FakeBluetoothDeviceBundle>(
        adapter_, heart_rate_device_bundle_data);

    // Hook up the test bluetooth delegate.
    old_browser_client_ = SetBrowserClientForTesting(&browser_client_);

    contents()->GetPrimaryMainFrame()->InitializeRenderFrameIfNeeded();

    // Navigate to a URL so that WebBluetoothServiceImpl::GetOrigin() returns a
    // valid origin. This is required when checking for Bluetooth permissions.
    constexpr char kTestURL[] = "https://my-battery-level.com";
    NavigationSimulator::NavigateAndCommitFromBrowser(contents(),
                                                      GURL(kTestURL));

    // Simulate a frame connected to a bluetooth service.
    mojo::PendingReceiver<blink::mojom::WebBluetoothService> receiver =
        service_.BindNewPipeAndPassReceiver();
    service_ptr_ = WebBluetoothServiceImpl::CreateForTesting(
        contents()->GetPrimaryMainFrame(), std::move(receiver));

    // GetAvailability connects the Web Bluetooth service to the adapter. Call
    // it twice in parallel to exercise what happens when multiple requests to
    // acquire the BluetoothAdapter are in flight.
    TestFuture<bool> future_1;
    TestFuture<bool> future_2;
    service_ptr_->GetAvailability(future_1.GetCallback());
    service_ptr_->GetAvailability(future_2.GetCallback());
    // Use Wait() instead of Get() because we don't care about the result.
    EXPECT_TRUE(future_1.Wait());
    EXPECT_TRUE(future_2.Wait());
  }

  void TearDown() override {
    adapter_.reset();
    battery_device_bundle_.reset();
    heart_rate_device_bundle_.reset();
    service_ptr_ = nullptr;
    SetBrowserClientForTesting(old_browser_client_);
    RenderViewHostImplTestHarness::TearDown();
  }

  mojo::PendingAssociatedRemote<WebBluetoothCharacteristicClient>
  BindCharacteristicClientAndPassRemote() {
    return characteristic_client_.BindNewEndpointClientAndPassRemote();
  }

 protected:
  blink::mojom::WebBluetoothLeScanFilterPtr CreateScanFilter(
      const std::string& name,
      const std::string& name_prefix) {
    std::optional<std::vector<device::BluetoothUUID>> services;
    services.emplace();
    services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
    return blink::mojom::WebBluetoothLeScanFilter::New(
        services, name, name_prefix, /*manufacturer_data=*/std::nullopt);
  }

  blink::mojom::WebBluetoothResult RequestScanningStartAndSimulatePromptEvent(
      const blink::mojom::WebBluetoothLeScanFilter& filter,
      FakeWebBluetoothAdvertisementClient* client,
      BluetoothScanningPrompt::Event event) {
    mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
        client_remote;
    client->BindReceiver(client_remote.InitWithNewEndpointAndPassReceiver());
    auto options = blink::mojom::WebBluetoothRequestLEScanOptions::New();
    options->filters.emplace();
    auto filter_ptr = blink::mojom::WebBluetoothLeScanFilter::New(
        filter.services, filter.name, filter.name_prefix,
        /*manufacturer_data=*/std::nullopt);
    options->filters->push_back(std::move(filter_ptr));

    // Use two RunLoops to guarantee the order of operations for this test.
    // |callback_loop| guarantees that RequestScanningStartCallback has finished
    // executing and |result| has been populated. |request_loop| ensures that
    // the entire RequestScanningStart flow has finished before the method
    // returns.
    base::RunLoop callback_loop, request_loop;
    blink::mojom::WebBluetoothResult result;
    service_ptr_->RequestScanningStart(
        std::move(client_remote), std::move(options),
        base::BindLambdaForTesting(
            [&callback_loop, &result](blink::mojom::WebBluetoothResult r) {
              result = std::move(r);
              callback_loop.Quit();
            }));

    // Post a task to simulate a prompt event during a call to
    // RequestScanningStart().
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindLambdaForTesting(
                       [&callback_loop, &event, &request_loop, this]() {
                         browser_client_.bluetooth_delegate()
                             ->RunBluetoothScanningPromptEventCallback(event);
                         callback_loop.Run();
                         request_loop.Quit();
                       }));
    request_loop.Run();
    return result;
  }

  void RegisterTestCharacteristic() {
    auto& battery_device_id = AddTestDevice(battery_device_bundle());

    auto& device = battery_device_bundle_->device();
    device.SetGattServicesDiscoveryComplete(true);

    FakeBluetoothCharacteristic& test_characteristic =
        battery_device_bundle().characteristic();

    {
      base::RunLoop run_loop;
      service_ptr_->RemoteServerGetPrimaryServices(
          battery_device_id, WebBluetoothGATTQueryQuantity::SINGLE,
          battery_device_bundle().service().GetUUID(),
          base::BindLambdaForTesting(
              [&run_loop](
                  WebBluetoothResult result,
                  std::optional<std::vector<WebBluetoothRemoteGATTServicePtr>>
                      services) {
                EXPECT_EQ(result, WebBluetoothResult::SUCCESS);
                run_loop.Quit();
              }));
      run_loop.Run();
    }

    {
      base::RunLoop run_loop;
      service_ptr_->RemoteServiceGetCharacteristics(
          battery_device_bundle().service().GetIdentifier(),
          WebBluetoothGATTQueryQuantity::SINGLE, test_characteristic.GetUUID(),
          base::BindLambdaForTesting(
              [&run_loop](
                  WebBluetoothResult result,
                  std::optional<
                      std::vector<WebBluetoothRemoteGATTCharacteristicPtr>>
                      characteristic) {
                EXPECT_EQ(result, WebBluetoothResult::SUCCESS);
                run_loop.Quit();
              }));
      run_loop.Run();
    }
  }

  FakeBluetoothDeviceBundle& battery_device_bundle() const {
    return *battery_device_bundle_;
  }

  FakeBluetoothDeviceBundle& heart_rate_device_bundle() const {
    return *heart_rate_device_bundle_;
  }

  const blink::WebBluetoothDeviceId& AddTestDevice(
      FakeBluetoothDeviceBundle& device_bundle) {
    auto device_options = blink::mojom::WebBluetoothRequestDeviceOptions::New();
    device_options->accept_all_devices = true;
    device_options->optional_services.push_back(
        device_bundle.service().GetUUID());
    return service_ptr_->allowed_devices().AddDevice(
        device_bundle.device().GetAddress(), device_options);
  }

  void DeleteService() {
    // This is a hack; destruction is normally implicitly triggered by
    // navigation or destruction of the frame itself, and not explicitly like
    // this test does.
    WebBluetoothServiceImpl::DeleteForCurrentDocument(
        &service_ptr_.ExtractAsDangling()->render_frame_host());
  }

  scoped_refptr<FakeBluetoothAdapter> adapter_;
  raw_ptr<WebBluetoothServiceImpl> service_ptr_ = nullptr;
  mojo::Remote<blink::mojom::WebBluetoothService> service_;
  TestContentBrowserClient browser_client_;
  raw_ptr<ContentBrowserClient> old_browser_client_ = nullptr;
  std::unique_ptr<FakeBluetoothDeviceBundle> battery_device_bundle_;
  std::unique_ptr<FakeBluetoothDeviceBundle> heart_rate_device_bundle_;
  FakeWebBluetoothCharacteristicClient characteristic_client_;
};

TEST_F(WebBluetoothServiceImplTest, DestroyedDuringRequestDevice) {
  auto options = blink::mojom::WebBluetoothRequestDeviceOptions::New();
  options->accept_all_devices = true;

  base::MockCallback<WebBluetoothServiceImpl::RequestDeviceCallback> callback;
  EXPECT_CALL(callback, Run).Times(0);
  service_ptr_->RequestDevice(std::move(options), callback.Get());

  base::RunLoop loop;
  DeleteService();
  loop.RunUntilIdle();
}

TEST_F(WebBluetoothServiceImplTest, PermissionAllowed) {
  blink::mojom::WebBluetoothLeScanFilterPtr filter = CreateScanFilter("a", "b");
  std::optional<WebBluetoothServiceImpl::ScanFilters> filters;
  filters.emplace();
  filters->push_back(filter.Clone());
  EXPECT_FALSE(service_ptr_->AreScanFiltersAllowed(filters));

  FakeWebBluetoothAdvertisementClient client;
  blink::mojom::WebBluetoothResult result =
      RequestScanningStartAndSimulatePromptEvent(
          *filter, &client, BluetoothScanningPrompt::Event::kAllow);
  EXPECT_EQ(result, blink::mojom::WebBluetoothResult::SUCCESS);
  // |filters| should be allowed.
  EXPECT_TRUE(service_ptr_->AreScanFiltersAllowed(filters));
}

TEST_F(WebBluetoothServiceImplTest, DestroyedDuringRequestScanningStart) {
  blink::mojom::WebBluetoothLeScanFilterPtr filter = CreateScanFilter("a", "b");
  std::optional<WebBluetoothServiceImpl::ScanFilters> filters;

  FakeWebBluetoothAdvertisementClient client;
  mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
      client_remote;
  client.BindReceiver(client_remote.InitWithNewEndpointAndPassReceiver());

  auto options = blink::mojom::WebBluetoothRequestLEScanOptions::New();
  options->filters.emplace();
  options->filters->push_back(std::move(filter));

  // The callback is currently called before delete is completed, during
  // the scanning request. Though, this is a behavior that is not mandatory
  // so not calling the callback would also be valid.
  base::RunLoop loop;
  base::MockCallback<WebBluetoothServiceImpl::RequestScanningStartCallback>
      callback;
  EXPECT_CALL(callback, Run).Times(1);
  service_ptr_->RequestScanningStart(std::move(client_remote),
                                     std::move(options), callback.Get());

  // Post a task to delete the WebBluetoothService state during a call to
  // RequestScanningStart().
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this]() { DeleteService(); }));

  loop.RunUntilIdle();
}

TEST_F(WebBluetoothServiceImplTest, PermissionPromptCanceled) {
  blink::mojom::WebBluetoothLeScanFilterPtr filter = CreateScanFilter("a", "b");
  std::optional<WebBluetoothServiceImpl::ScanFilters> filters;
  filters.emplace();
  filters->push_back(filter.Clone());
  EXPECT_FALSE(service_ptr_->AreScanFiltersAllowed(filters));

  FakeWebBluetoothAdvertisementClient client;
  blink::mojom::WebBluetoothResult result =
      RequestScanningStartAndSimulatePromptEvent(
          *filter, &client, BluetoothScanningPrompt::Event::kCanceled);

  EXPECT_EQ(blink::mojom::WebBluetoothResult::PROMPT_CANCELED, result);
  // |filters| should still not be allowed.
  EXPECT_FALSE(service_ptr_->AreScanFiltersAllowed(filters));
}

TEST_F(WebBluetoothServiceImplTest,
       BluetoothScanningPermissionRevokedWhenTabHidden) {
  blink::mojom::WebBluetoothLeScanFilterPtr filter = CreateScanFilter("a", "b");
  std::optional<WebBluetoothServiceImpl::ScanFilters> filters;
  filters.emplace();
  filters->push_back(filter.Clone());
  FakeWebBluetoothAdvertisementClient client;
  blink::mojom::WebBluetoothResult result =
      RequestScanningStartAndSimulatePromptEvent(
          *filter, &client, BluetoothScanningPrompt::Event::kAllow);
  EXPECT_EQ(result, blink::mojom::WebBluetoothResult::SUCCESS);
  EXPECT_TRUE(service_ptr_->AreScanFiltersAllowed(filters));

  contents()->SetVisibilityAndNotifyObservers(Visibility::HIDDEN);

  // The previously granted Bluetooth scanning permission should be revoked.
  EXPECT_FALSE(service_ptr_->AreScanFiltersAllowed(filters));
}

TEST_F(WebBluetoothServiceImplTest,
       BluetoothScanningPermissionRevokedWhenTabOccluded) {
  blink::mojom::WebBluetoothLeScanFilterPtr filter = CreateScanFilter("a", "b");
  std::optional<WebBluetoothServiceImpl::ScanFilters> filters;
  filters.emplace();
  filters->push_back(filter.Clone());
  FakeWebBluetoothAdvertisementClient client;
  RequestScanningStartAndSimulatePromptEvent(
      *filter, &client, BluetoothScanningPrompt::Event::kAllow);
  EXPECT_TRUE(service_ptr_->AreScanFiltersAllowed(filters));

  contents()->SetVisibilityAndNotifyObservers(Visibility::OCCLUDED);

  // The previously granted Bluetooth scanning permission should be revoked.
  EXPECT_FALSE(service_ptr_->AreScanFiltersAllowed(filters));
}

TEST_F(WebBluetoothServiceImplTest,
       BluetoothScanningPermissionRevokedWhenFocusIsLost) {
  blink::mojom::WebBluetoothLeScanFilterPtr filter = CreateScanFilter("a", "b");
  std::optional<WebBluetoothServiceImpl::ScanFilters> filters;
  filters.emplace();
  filters->push_back(filter.Clone());
  FakeWebBluetoothAdvertisementClient client;
  RequestScanningStartAndSimulatePromptEvent(
      *filter, &client, BluetoothScanningPrompt::Event::kAllow);
  EXPECT_TRUE(service_ptr_->AreScanFiltersAllowed(filters));

  main_test_rfh()->GetRenderWidgetHost()->LostFocus();

  // The previously granted Bluetooth scanning permission should be revoked.
  EXPECT_FALSE(service_ptr_->AreScanFiltersAllowed(filters));
}

TEST_F(WebBluetoothServiceImplTest,
       BluetoothScanningPermissionRevokedWhenBlocked) {
  blink::mojom::WebBluetoothLeScanFilterPtr filter_1 =
      CreateScanFilter("a", "b");
  std::optional<WebBluetoothServiceImpl::ScanFilters> filters_1;
  filters_1.emplace();
  filters_1->push_back(filter_1.Clone());
  FakeWebBluetoothAdvertisementClient client_1;
  blink::mojom::WebBluetoothResult result_1 =
      RequestScanningStartAndSimulatePromptEvent(
          *filter_1, &client_1, BluetoothScanningPrompt::Event::kAllow);
  EXPECT_EQ(result_1, blink::mojom::WebBluetoothResult::SUCCESS);
  EXPECT_TRUE(service_ptr_->AreScanFiltersAllowed(filters_1));
  EXPECT_FALSE(client_1.on_connection_error_called());

  blink::mojom::WebBluetoothLeScanFilterPtr filter_2 =
      CreateScanFilter("c", "d");
  std::optional<WebBluetoothServiceImpl::ScanFilters> filters_2;
  filters_2.emplace();
  filters_2->push_back(filter_2.Clone());
  FakeWebBluetoothAdvertisementClient client_2;
  blink::mojom::WebBluetoothResult result_2 =
      RequestScanningStartAndSimulatePromptEvent(
          *filter_2, &client_2, BluetoothScanningPrompt::Event::kAllow);
  EXPECT_EQ(result_2, blink::mojom::WebBluetoothResult::SUCCESS);
  EXPECT_TRUE(service_ptr_->AreScanFiltersAllowed(filters_2));
  EXPECT_FALSE(client_2.on_connection_error_called());

  blink::mojom::WebBluetoothLeScanFilterPtr filter_3 =
      CreateScanFilter("e", "f");
  std::optional<WebBluetoothServiceImpl::ScanFilters> filters_3;
  filters_3.emplace();
  filters_3->push_back(filter_3.Clone());
  FakeWebBluetoothAdvertisementClient client_3;
  blink::mojom::WebBluetoothResult result_3 =
      RequestScanningStartAndSimulatePromptEvent(
          *filter_3, &client_3, BluetoothScanningPrompt::Event::kBlock);
  EXPECT_EQ(blink::mojom::WebBluetoothResult::SCANNING_BLOCKED, result_3);
  EXPECT_FALSE(service_ptr_->AreScanFiltersAllowed(filters_3));

  // The previously granted Bluetooth scanning permission should be revoked.
  EXPECT_FALSE(service_ptr_->AreScanFiltersAllowed(filters_1));
  EXPECT_FALSE(service_ptr_->AreScanFiltersAllowed(filters_2));

  base::RunLoop().RunUntilIdle();

  // All existing scanning clients are disconnected.
  EXPECT_TRUE(client_1.on_connection_error_called());
  EXPECT_TRUE(client_2.on_connection_error_called());
  EXPECT_TRUE(client_3.on_connection_error_called());
}

TEST_F(WebBluetoothServiceImplTest,
       ReadCharacteristicValueErrorWithValueIgnored) {
  // The contract for calls accepting a
  // BluetoothRemoteGattCharacteristic::ValueCallback callback argument is that
  // when an error occurs, value must be ignored. This test verifies that
  // WebBluetoothServiceImpl::OnCharacteristicReadValue honors that contract
  // and will not pass a value to its callback
  // (a RemoteCharacteristicReadValueCallback instance) when an error occurs
  // with a non-empty value array.
  const std::vector<uint8_t> read_error_value = {1, 2, 3};
  bool callback_called = false;
  const std::string characteristic_instance_id = "fake-id";
  service_ptr_->OnCharacteristicReadValue(
      characteristic_instance_id,
      base::BindLambdaForTesting(
          [&callback_called](blink::mojom::WebBluetoothResult result,
                             const std::optional<std::vector<uint8_t>>& value) {
            callback_called = true;
            EXPECT_EQ(
                blink::mojom::WebBluetoothResult::GATT_OPERATION_IN_PROGRESS,
                result);
            EXPECT_FALSE(value.has_value());
          }),
      device::BluetoothGattService::GattErrorCode::kInProgress,
      read_error_value);
  EXPECT_TRUE(callback_called);

  // This test doesn't invoke any methods of the mock adapter. Allow it to be
  // leaked without producing errors.
  Mock::AllowLeak(adapter_.get());
}

#if PAIR_BLUETOOTH_ON_DEMAND()
TEST_F(WebBluetoothServiceImplTest, ReadCharacteristicValueNotAuthorized) {
  const std::vector<uint8_t> read_error_value = {1, 2, 3};
  bool read_value_callback_called = false;

  RegisterTestCharacteristic();
  const FakeBluetoothCharacteristic& test_characteristic =
      battery_device_bundle().characteristic();

  MockWebBluetoothPairingManager* pairing_manager =
      new MockWebBluetoothPairingManager();
  service_ptr_->SetPairingManagerForTesting(
      std::unique_ptr<WebBluetoothPairingManager>(pairing_manager));

  EXPECT_CALL(*pairing_manager, PairForCharacteristicReadValue(_, _)).Times(1);

  service_ptr_->OnCharacteristicReadValue(
      test_characteristic.GetIdentifier(),
      base::BindLambdaForTesting(
          [&read_value_callback_called](
              blink::mojom::WebBluetoothResult result,
              const std::optional<std::vector<uint8_t>>& value) {
            read_value_callback_called = true;
            EXPECT_EQ(blink::mojom::WebBluetoothResult::GATT_NOT_AUTHORIZED,
                      result);
            EXPECT_FALSE(value.has_value());
          }),
      device::BluetoothGattService::GattErrorCode::kNotAuthorized,
      read_error_value);
  EXPECT_FALSE(read_value_callback_called);
}

TEST_F(WebBluetoothServiceImplTest, IncompletePairingOnShutdown) {
  RegisterTestCharacteristic();

  EXPECT_CALL(battery_device_bundle().characteristic(),
              ReadRemoteCharacteristic_(_))
      .WillOnce(base::test::RunOnceCallback<0>(
          device::BluetoothGattService::GattErrorCode::kNotAuthorized,
          std::vector<uint8_t>()));

  base::MockCallback<
      WebBluetoothServiceImpl::RemoteCharacteristicReadValueCallback>
      callback;

  // The pairing is never completed so the callback won't be run before the
  // test ends.
  EXPECT_CALL(callback, Run(_, _)).Times(0);

  service_ptr_->RemoteCharacteristicReadValue(
      battery_device_bundle().characteristic().GetIdentifier(), callback.Get());

  // Simulate the WebBluetoothServiceImpl being destroyed due to a navigation or
  // tab closure while the pairing request is in progress.
  DeleteService();
}
#endif  // PAIR_BLUETOOTH_ON_DEMAND()

TEST_F(WebBluetoothServiceImplTest, DeferredStartNotifySession) {
  RegisterTestCharacteristic();
  FakeBluetoothCharacteristic& test_characteristic =
      battery_device_bundle().characteristic();

  // Test both failing.
  {
    base::RunLoop run_loop;
    int outstanding_callbacks = 2;

    test_characteristic.DeferNextStartNotification(
        BluetoothGattService::GattErrorCode::kFailed);

    auto callback = base::BindLambdaForTesting(
        [&run_loop, &outstanding_callbacks](WebBluetoothResult result) {
          EXPECT_EQ(result, WebBluetoothResult::GATT_UNKNOWN_FAILURE);
          if (--outstanding_callbacks == 0) {
            run_loop.Quit();
          }
        });
    service_ptr_->RemoteCharacteristicStartNotifications(
        test_characteristic.GetIdentifier(),
        BindCharacteristicClientAndPassRemote(), callback);

    service_ptr_->RemoteCharacteristicStartNotifications(
        test_characteristic.GetIdentifier(),
        BindCharacteristicClientAndPassRemote(), callback);

    test_characteristic.ResumeDeferredStartNotification();

    run_loop.Run();
  }

  // Test both succeeding.
  {
    base::RunLoop run_loop;
    int outstanding_callbacks = 2;

    test_characteristic.DeferNextStartNotification(
        /*error_code=*/std::nullopt);

    auto callback = base::BindLambdaForTesting(
        [&run_loop, &outstanding_callbacks](WebBluetoothResult result) {
          EXPECT_EQ(result, WebBluetoothResult::SUCCESS);
          if (--outstanding_callbacks == 0) {
            run_loop.Quit();
          }
        });
    service_ptr_->RemoteCharacteristicStartNotifications(
        test_characteristic.GetIdentifier(),
        BindCharacteristicClientAndPassRemote(), callback);

    service_ptr_->RemoteCharacteristicStartNotifications(
        test_characteristic.GetIdentifier(),
        BindCharacteristicClientAndPassRemote(), callback);

    test_characteristic.ResumeDeferredStartNotification();

    run_loop.Run();
  }
}

TEST_F(WebBluetoothServiceImplTest, DeviceGattServicesDiscoveryTimeout) {
  auto& battery_device_id = AddTestDevice(battery_device_bundle());

  auto& device = battery_device_bundle_->device();
  device.SetGattServicesDiscoveryComplete(false);

  TestFuture<WebBluetoothResult,
             std::optional<std::vector<WebBluetoothRemoteGATTServicePtr>>>
      get_primary_services_future;
  service_ptr_->RemoteServerGetPrimaryServices(
      battery_device_id, WebBluetoothGATTQueryQuantity::SINGLE,
      battery_device_bundle().service().GetUUID(),
      get_primary_services_future.GetCallback());
  device.SetConnected(false);
  service_ptr_->DeviceChanged(device.GetAdapter(), &device);
  EXPECT_EQ(get_primary_services_future.Get<0>(),
            blink::mojom::WebBluetoothResult::NO_SERVICES_FOUND);
}

TEST_F(WebBluetoothServiceImplTest, DeviceDisconnected) {
  auto& battery_device_id = AddTestDevice(battery_device_bundle());

  auto& device = battery_device_bundle_->device();
  device.SetConnected(false);

  TestFuture<WebBluetoothResult,
             std::optional<std::vector<WebBluetoothRemoteGATTServicePtr>>>
      get_primary_services_future;
  service_ptr_->RemoteServerGetPrimaryServices(
      battery_device_id, WebBluetoothGATTQueryQuantity::SINGLE,
      battery_device_bundle().service().GetUUID(),
      get_primary_services_future.GetCallback());
  EXPECT_EQ(get_primary_services_future.Get<0>(),
            blink::mojom::WebBluetoothResult::NO_SERVICES_FOUND);
}

TEST_F(WebBluetoothServiceImplTest, RejectOpaqueOrigin) {
  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(std::string());
  response_headers->SetHeader("Content-Security-Policy",
                              "sandbox allow-scripts");
  // The WebBluetoothService lifetime is tied to the document it was created in.
  // A navigation is going to remove it. So it must be cleared to avoid the
  // pointer to dangle.
  service_ptr_ = nullptr;

  auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
      GURL("http://whatever.com"), main_test_rfh());
  navigation_simulator->SetResponseHeaders(response_headers);
  navigation_simulator->Start();
  navigation_simulator->Commit();

  mojo::Remote<blink::mojom::WebBluetoothService> service;
  WebBluetoothServiceImpl::BindIfAllowed(contents()->GetPrimaryMainFrame(),
                                         service.BindNewPipeAndPassReceiver());

  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Web Bluetooth is not allowed from an opaque origin.");
}

TEST_F(WebBluetoothServiceImplTest, TwoWatchAdvertisementsReqSuccess) {
  TestFuture<WebBluetoothResult> future1;
  TestFuture<WebBluetoothResult> future2;

  auto& battry_device_id = AddTestDevice(battery_device_bundle());
  auto& heart_rate_device_id = AddTestDevice(heart_rate_device_bundle());

  mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
      client_remote1;
  mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
      client_remote2;

  battery_device_bundle().advertisement_client().BindReceiver(
      client_remote1.InitWithNewEndpointAndPassReceiver());
  heart_rate_device_bundle().advertisement_client().BindReceiver(
      client_remote2.InitWithNewEndpointAndPassReceiver());

  // Install SUCCESS result for StartScanWithFilter
  adapter_->SetStartScanWithFilterResult(
      device::UMABluetoothDiscoverySessionOutcome::SUCCESS);

  service_ptr_->WatchAdvertisementsForDevice(
      battry_device_id, std::move(client_remote1), future1.GetCallback());
  service_ptr_->WatchAdvertisementsForDevice(
      heart_rate_device_id, std::move(client_remote2), future2.GetCallback());

  EXPECT_EQ(future1.Get(), WebBluetoothResult::SUCCESS);
  EXPECT_EQ(future2.Get(), WebBluetoothResult::SUCCESS);

  // StopScan call from device::BluetoothDiscoverySession dtor is expected while
  // tearing down the test.
  EXPECT_CALL(*adapter_, StopScan).Times(1);
}

TEST_F(WebBluetoothServiceImplTest, TwoWatchAdvertisementsReqFail) {
  TestFuture<WebBluetoothResult> future1;
  TestFuture<WebBluetoothResult> future2;

  auto& battry_device_id = AddTestDevice(battery_device_bundle());
  auto& heart_rate_device_id = AddTestDevice(heart_rate_device_bundle());

  mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
      client_remote1;
  mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
      client_remote2;

  battery_device_bundle().advertisement_client().BindReceiver(
      client_remote1.InitWithNewEndpointAndPassReceiver());
  heart_rate_device_bundle().advertisement_client().BindReceiver(
      client_remote2.InitWithNewEndpointAndPassReceiver());

  // Install FAILED result for StartScanWithFilter
  adapter_->SetStartScanWithFilterResult(
      device::UMABluetoothDiscoverySessionOutcome::FAILED);

  service_ptr_->WatchAdvertisementsForDevice(
      battry_device_id, std::move(client_remote1), future1.GetCallback());
  service_ptr_->WatchAdvertisementsForDevice(
      heart_rate_device_id, std::move(client_remote2), future2.GetCallback());

  EXPECT_EQ(future1.Get(), WebBluetoothResult::NO_BLUETOOTH_ADAPTER);
  EXPECT_EQ(future2.Get(), WebBluetoothResult::NO_BLUETOOTH_ADAPTER);
}

TEST_F(WebBluetoothServiceImplTest,
       SecWatchAdvertisementsReqAfterFirstSuccess) {
  // Install SUCCESS result for StartScanWithFilter
  adapter_->SetStartScanWithFilterResult(
      device::UMABluetoothDiscoverySessionOutcome::SUCCESS);

  TestFuture<WebBluetoothResult> future1;
  auto& battry_device_id = AddTestDevice(battery_device_bundle());
  mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
      client_remote1;
  battery_device_bundle().advertisement_client().BindReceiver(
      client_remote1.InitWithNewEndpointAndPassReceiver());
  service_ptr_->WatchAdvertisementsForDevice(
      battry_device_id, std::move(client_remote1), future1.GetCallback());
  EXPECT_EQ(future1.Get(), WebBluetoothResult::SUCCESS);

  // When second watchAdvertisements request comes in and there is an active
  // discovery session, it should get SUCCESS result directly.
  TestFuture<WebBluetoothResult> future2;
  auto& heart_rate_device_id = AddTestDevice(heart_rate_device_bundle());
  mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
      client_remote2;
  heart_rate_device_bundle().advertisement_client().BindReceiver(
      client_remote2.InitWithNewEndpointAndPassReceiver());

  // Install FAILED result for StartScanWithFilter.
  // This is to ensure that the second request succeed for an active discovery
  // session instead of going through entire StartScanWithFilter call.
  adapter_->SetStartScanWithFilterResult(
      device::UMABluetoothDiscoverySessionOutcome::FAILED);
  service_ptr_->WatchAdvertisementsForDevice(
      heart_rate_device_id, std::move(client_remote2), future2.GetCallback());
  EXPECT_EQ(future2.Get(), WebBluetoothResult::SUCCESS);

  // StopScan call from device::BluetoothDiscoverySession dtor is expected while
  // tearing down the test.
  EXPECT_CALL(*adapter_, StopScan).Times(1);
}

TEST_F(WebBluetoothServiceImplTest, ServiceDestroyedDuringAdapterAcquisition) {
  // Remove the adapter configured by the base test to ensure an async
  // AcquireAdapter flow.
  BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterOverride(nullptr);

  // Due to the service being destroyed before acquisition, this adapter will
  // never receive these observer calls.
  EXPECT_CALL(*adapter_, AddObserver).Times(0);
  EXPECT_CALL(*adapter_, RemoveObserver).Times(0);

  BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterOverride(adapter_);

  // Post a task that destroys the service during adapter acquisition.
  // This is a hack; destruction is normally implicitly triggered by
  // navigation or destruction of the frame itself, and not explicitly
  // like this test does.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        WebBluetoothServiceImpl::DeleteForCurrentDocument(
            &service_ptr_.ExtractAsDangling()->render_frame_host());
      }));

  // GetAvailability connects the Web Bluetooth service to the adapter,
  // running through the AcquireAdapter flow.
  TestFuture<bool> future_1;
  service_ptr_->GetAvailability(future_1.GetCallback());
  EXPECT_TRUE(future_1.Wait());
  BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterOverride(nullptr);
}

class WebBluetoothServiceImplTestWithBaseAdapter
    : public RenderViewHostImplTestHarness,
      public WithParamInterface<bool> {
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    // Set up a fake system adapter.
    base_adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    EXPECT_CALL(*base_adapter_, IsPresent()).WillRepeatedly(Return(true));
    BluetoothAdapterFactoryWrapper::Get().SetAdapterInternal(
        base_adapter_, /*is_override_adapter=*/false);

    // Hook up the test bluetooth delegate.
    old_browser_client_ = SetBrowserClientForTesting(&browser_client_);
    contents()->GetPrimaryMainFrame()->InitializeRenderFrameIfNeeded();

    // Navigate to a URL so that WebBluetoothServiceImpl::GetOrigin() returns a
    // valid origin. This is required when checking for Bluetooth permissions.
    constexpr char kTestURL[] = "https://my-battery-level.com";
    NavigationSimulator::NavigateAndCommitFromBrowser(contents(),
                                                      GURL(kTestURL));

    // Set up an adapter.
    mojo::PendingReceiver<blink::mojom::WebBluetoothService> receiver =
        service_.BindNewPipeAndPassReceiver();
    service_ptr_ = WebBluetoothServiceImpl::CreateForTesting(
        contents()->GetPrimaryMainFrame(), std::move(receiver));

    // GetAvailability connects the Web Bluetooth service to the adapter. Call
    // it twice in parallel to exercise what happens when multiple requests to
    // acquire the BluetoothAdapter are in flight.
    TestFuture<bool> future_1;
    TestFuture<bool> future_2;
    service_ptr_->GetAvailability(future_1.GetCallback());
    service_ptr_->GetAvailability(future_2.GetCallback());
    EXPECT_TRUE(future_1.Wait());
    EXPECT_TRUE(future_2.Wait());
  }

  void TearDown() override {
    base_adapter_.reset();
    service_ptr_ = nullptr;
    SetBrowserClientForTesting(old_browser_client_);
    RenderViewHostImplTestHarness::TearDown();
  }

 protected:
  scoped_refptr<FakeBluetoothAdapter> base_adapter_;
  raw_ptr<WebBluetoothServiceImpl> service_ptr_ = nullptr;
  mojo::Remote<blink::mojom::WebBluetoothService> service_;
  TestContentBrowserClient browser_client_;
  raw_ptr<ContentBrowserClient> old_browser_client_ = nullptr;
};

TEST_F(WebBluetoothServiceImplTestWithBaseAdapter,
       EmulatedAdapterRemovalRestoresOriginalAdapter) {
  // Confirm that the system adapter is being used.
  EXPECT_EQ(BluetoothAdapterFactoryWrapper::Get().GetAdapter(service_ptr_),
            base_adapter_);

  // Create an override adapter and configure to use it.
  scoped_refptr<FakeBluetoothAdapter> override_adapter_ =
      base::MakeRefCounted<FakeBluetoothAdapter>();
  EXPECT_CALL(*override_adapter_, IsPresent()).WillRepeatedly(Return(true));
  BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterOverride(
      override_adapter_);

  // Confirm that the override adapter is being used.
  EXPECT_EQ(BluetoothAdapterFactoryWrapper::Get().GetAdapter(service_ptr_),
            override_adapter_);

  // Remove the override and confirm that it returns to the system adapter.
  BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterOverride(nullptr);
  EXPECT_EQ(BluetoothAdapterFactoryWrapper::Get().GetAdapter(service_ptr_),
            base_adapter_);
}
}  // namespace content
