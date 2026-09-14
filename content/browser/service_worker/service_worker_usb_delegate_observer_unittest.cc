// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_usb_delegate_observer.h"

#include <cstddef>
#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_device_delegate_observer_unittest.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/usb/usb_test_utils.h"
#include "content/browser/usb/web_usb_service_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/usb_delegate.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

using ::base::test::RunClosure;
using ::base::test::TestFuture;
using ::testing::_;
using ::testing::Ref;
using ::testing::Return;

const char kTestUrl[] = "https://www.google.com";

// This TestServiceWorkerObserver observes starting, started, and stopped of
// the worker with `version_id`.
class TestServiceWorkerObserver : public ServiceWorkerContextCoreObserver {
 public:
  TestServiceWorkerObserver(ServiceWorkerContextWrapper* context,
                            int64_t version_id)
      : version_id_(version_id) {
    observation_.Observe(context);
  }

  TestServiceWorkerObserver(const TestServiceWorkerObserver&) = delete;
  TestServiceWorkerObserver& operator=(const TestServiceWorkerObserver&) =
      delete;

  ~TestServiceWorkerObserver() override = default;

  void WaitForWorkerStarting() { starting_run_loop.Run(); }

  void WaitForWorkerStarted() { started_run_loop.Run(); }

  void WaitForWorkerStopped() { stopped_run_loop.Run(); }

  // ServiceWorkerContextCoreObserver:
  void OnStarting(int64_t version_id) override {
    if (version_id != version_id_) {
      return;
    }
    starting_run_loop.Quit();
  }

  void OnStarted(int64_t version_id,
                 const GURL& scope,
                 ChildProcessId process_id,
                 const GURL& script_url,
                 const blink::ServiceWorkerToken& token,
                 const blink::StorageKey& key) override {
    if (version_id != version_id_) {
      return;
    }
    started_run_loop.Quit();
  }

  void OnStopped(int64_t version_id) override {
    if (version_id != version_id_) {
      return;
    }
    stopped_run_loop.Quit();
  }

 private:
  base::RunLoop starting_run_loop;
  base::RunLoop started_run_loop;
  base::RunLoop stopped_run_loop;
  int64_t version_id_;
  base::ScopedObservation<ServiceWorkerContextWrapper,
                          ServiceWorkerContextCoreObserver>
      observation_{this};
};

class ServiceWorkerUsbDelegateObserverTest
    : public content::ServiceWorkerDeviceDelegateObserverTest {
 public:
  ServiceWorkerUsbDelegateObserverTest() = default;
  ServiceWorkerUsbDelegateObserverTest(ServiceWorkerUsbDelegateObserverTest&) =
      delete;
  ServiceWorkerUsbDelegateObserverTest& operator=(
      ServiceWorkerUsbDelegateObserverTest&) = delete;
  ~ServiceWorkerUsbDelegateObserverTest() override = default;

  void SetUp() override {
    content::ServiceWorkerDeviceDelegateObserverTest::SetUp();
    usb_delegate().SetAssertBrowserContext(true);

    // Connect with the FakeUsbDeviceManager.
    mojo::PendingRemote<device::mojom::UsbDeviceManager> pending_device_manager;
    usb_device_manager_.AddReceiver(
        pending_device_manager.InitWithNewPipeAndPassReceiver());

    // For tests, all devices are permitted by default.
    ON_CALL(usb_delegate(), HasDevicePermission).WillByDefault(Return(true));
    ON_CALL(usb_delegate(), PageMayUseUsb).WillByDefault(Return(true));

    // Forward calls to the fake device manager.
    ON_CALL(usb_delegate(), GetDevices)
        .WillByDefault(
            [this](
                auto* browser_context,
                device::mojom::UsbDeviceManager::GetDevicesCallback callback) {
              usb_device_manager_.GetDevices(nullptr, std::move(callback));
            });
    ON_CALL(usb_delegate(), GetDevice)
        .WillByDefault(
            [this](
                auto* browser_context, const std::string& guid,
                base::span<const uint8_t> blocked_interface_classes,
                mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
                mojo::PendingRemote<device::mojom::UsbDeviceClient>
                    device_client) {
              usb_device_manager_.GetDevice(
                  guid,
                  std::vector<uint8_t>(blocked_interface_classes.begin(),
                                       blocked_interface_classes.end()),
                  std::move(device_receiver), std::move(device_client));
            });
    ON_CALL(usb_delegate(), GetDeviceInfo)
        .WillByDefault([this](auto* browser_context, const std::string& guid) {
          return usb_device_manager_.GetDeviceInfo(guid);
        });
  }

  void RegisterUsbManagerClient(
      const mojo::Remote<blink::mojom::WebUsbService>& service,
      MockDeviceManagerClient& device_manager_client) {
    service->SetClient(device_manager_client.CreateInterfacePtrAndBind());
    FlushUsbServicePipe(service);
  }

  device::mojom::UsbDeviceInfoPtr ConnectDevice(
      scoped_refptr<device::FakeUsbDeviceInfo> device,
      device::MockUsbMojoDevice* mock_device) {
    auto device_info = usb_device_manager_.AddDevice(std::move(device));
    if (mock_device) {
      usb_device_manager_.SetMockForDevice(device_info->guid, mock_device);
    }
    usb_delegate().OnDeviceAdded(*device_info);
    return device_info;
  }

  void DisconnectDevice(scoped_refptr<device::FakeUsbDeviceInfo> device) {
    auto device_info = device->GetDeviceInfo().Clone();
    usb_device_manager_.RemoveDevice(std::move(device));
    usb_delegate().OnDeviceRemoved(*device_info);
  }

  scoped_refptr<device::FakeUsbDeviceInfo> CreateFakeDevice() {
    return base::MakeRefCounted<device::FakeUsbDeviceInfo>(
        0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  }

  device::mojom::UsbOpenDeviceResultPtr NewUsbOpenDeviceSuccess() {
    return device::mojom::UsbOpenDeviceResult::NewSuccess(
        device::mojom::UsbOpenDeviceSuccess::OK);
  }

  mojo::Remote<blink::mojom::WebUsbService> CreateUsbService(
      ServiceWorkerVersion* version) {
    auto const& origin = version->key().origin();
    mojo::Remote<blink::mojom::WebUsbService> service;
    EXPECT_CALL(usb_delegate(), IsServiceWorkerAllowedForOrigin(origin))
        .Times(2)
        .WillRepeatedly(Return(true));
    auto* embedded_worker = version->embedded_worker();
    embedded_worker->BindUsbService(origin,
                                    service.BindNewPipeAndPassReceiver());
    return service;
  }

  void FlushUsbServicePipe(
      const mojo::Remote<blink::mojom::WebUsbService>& usb_service) {
    // Run GetDevices to flush mojo request.
    TestFuture<std::vector<device::mojom::UsbDeviceInfoPtr>> devices_future;
    usb_service->GetDevices(devices_future.GetCallback());
    EXPECT_TRUE(devices_future.Wait());
  }

  MockUsbDelegate& usb_delegate() { return test_client_.delegate(); }

  device::FakeUsbDeviceManager& usb_device_manager() {
    return usb_device_manager_;
  }

  MockDeviceManagerClient& device_manager_client() {
    return device_manager_client_;
  }

  std::tuple<blink::ServiceWorkerStatusCode,
             scoped_refptr<ServiceWorkerRegistration>>
  FindRegistration(int64_t registration_id, const blink::StorageKey& key) {
    TestFuture<blink::ServiceWorkerStatusCode,
               scoped_refptr<ServiceWorkerRegistration>>
        future;
    registry().FindRegistrationForId(registration_id, key,
                                     future.GetCallback());
    return future.Take();
  }

  void ServiceWorkerInstalling(
      scoped_refptr<ServiceWorkerVersion> version) override {
    // This simulates the scenario where the service worker script has an USB
    // event handler.
    version->set_has_usb_event_handlers(true);
  }

 protected:
  MockDeviceManagerClient device_manager_client_;
  UsbTestContentBrowserClient test_client_;
  device::FakeUsbDeviceManager usb_device_manager_;
  ScopedContentBrowserClientSetting setting{&test_client_};
};

class ServiceWorkerUsbDelegateObserverNoEventHandlersTest
    : public ServiceWorkerUsbDelegateObserverTest {
 public:
  void ServiceWorkerInstalling(
      scoped_refptr<ServiceWorkerVersion> version) override {
    // Do nothing to simulate no USB event handlers.
  }
};

}  // namespace

TEST_F(ServiceWorkerUsbDelegateObserverTest, OnDeviceAdded) {
  size_t num_workers = 10;
  std::vector<GURL> origins;
  std::vector<scoped_refptr<ServiceWorkerRegistration>> registrations;
  std::vector<int64_t> version_ids;
  std::vector<mojo::Remote<blink::mojom::WebUsbService>> usb_services(
      num_workers);
  std::vector<MockDeviceManagerClient> device_manager_clients(num_workers);
  for (size_t idx = 0; idx < num_workers; ++idx) {
    origins.push_back(
        GURL(base::StringPrintf("https://www.example%zu.com", idx)));
    registrations.push_back(InstallServiceWorker(origins[idx]));
    auto* version = registrations[idx]->newest_installed_version();
    ASSERT_NE(version, nullptr);
    version_ids.push_back(version->version_id());
  }

  mojo::Remote<device::mojom::UsbDevice> device1;
  mojo::Remote<device::mojom::UsbDevice> device2;
  device::MockUsbMojoDevice mock_device1;
  device::MockUsbMojoDevice mock_device2;
  auto fake_device_info1 = CreateFakeDevice();
  auto fake_device_info2 = CreateFakeDevice();

  // DeviceAdded event when the service worker is not running.
  {
    std::vector<std::unique_ptr<TestServiceWorkerObserver>>
        service_worker_observers;
    std::vector<TestFuture<device::mojom::UsbDeviceInfoPtr>>
        device_added_futures(num_workers);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      service_worker_observers.push_back(
          std::make_unique<TestServiceWorkerObserver>(context()->wrapper(),
                                                      version_ids[idx]));
      auto& device_added_future = device_added_futures[idx];
      EXPECT_CALL(device_manager_clients[idx], OnDeviceAdded)
          .WillOnce(
              [&](auto d) { device_added_future.SetValue(std::move(d)); });
    }
    auto device_info1 = ConnectDevice(fake_device_info1, &mock_device1);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      service_worker_observers[idx]->WaitForWorkerStarting();
      auto* version = context()->GetLiveVersion(version_ids[idx]);
      ASSERT_NE(version, nullptr);
      usb_services[idx] = CreateUsbService(version);
      RegisterUsbManagerClient(usb_services[idx], device_manager_clients[idx]);
      service_worker_observers[idx]->WaitForWorkerStarted();
      EXPECT_EQ(device_added_futures[idx].Get()->guid, device_info1->guid);
    }
  }

  // DeviceAdded event when the service worker is running.
  {
    std::vector<TestFuture<device::mojom::UsbDeviceInfoPtr>>
        device_added_futures(num_workers);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      auto& device_added_future = device_added_futures[idx];
      auto* version = context()->GetLiveVersion(version_ids[idx]);
      ASSERT_NE(version, nullptr);
      EXPECT_EQ(version->running_status(),
                blink::EmbeddedWorkerStatus::kRunning);
      EXPECT_CALL(device_manager_clients[idx], OnDeviceAdded)
          .WillOnce(
              [&](auto d) { device_added_future.SetValue(std::move(d)); });
    }
    auto device_info2 = ConnectDevice(fake_device_info2, &mock_device2);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      EXPECT_EQ(device_added_futures[idx].Get()->guid, device_info2->guid);
    }
  }
}

TEST_F(ServiceWorkerUsbDelegateObserverTest, OnDeviceRemoved) {
  size_t num_workers = 10;
  std::vector<GURL> origins;
  std::vector<scoped_refptr<ServiceWorkerRegistration>> registrations;
  std::vector<int64_t> version_ids;
  std::vector<mojo::Remote<blink::mojom::WebUsbService>> usb_services(
      num_workers);
  std::vector<MockDeviceManagerClient> device_manager_clients(num_workers);
  for (size_t idx = 0; idx < num_workers; ++idx) {
    origins.push_back(
        GURL(base::StringPrintf("https://www.example%zu.com", idx)));
    registrations.push_back(InstallServiceWorker(origins[idx]));
    auto* version = registrations[idx]->newest_installed_version();
    ASSERT_NE(version, nullptr);
    version_ids.push_back(version->version_id());
  }

  auto fake_device_info = CreateFakeDevice();
  usb_device_manager_.AddDevice(fake_device_info);

  // DeviceRemoved event when the service worker is not running.
  {
    std::vector<std::unique_ptr<TestServiceWorkerObserver>>
        service_worker_observers;
    std::vector<TestFuture<device::mojom::UsbDeviceInfoPtr>>
        device_removed_futures(num_workers);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      service_worker_observers.push_back(
          std::make_unique<TestServiceWorkerObserver>(context()->wrapper(),
                                                      version_ids[idx]));
      auto& device_removed_future = device_removed_futures[idx];
      EXPECT_CALL(device_manager_clients[idx], OnDeviceRemoved)
          .WillOnce(
              [&](auto d) { device_removed_future.SetValue(std::move(d)); });
    }
    DisconnectDevice(fake_device_info);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      service_worker_observers[idx]->WaitForWorkerStarting();
      auto* version = context()->GetLiveVersion(version_ids[idx]);
      ASSERT_NE(version, nullptr);
      usb_services[idx] = CreateUsbService(version);
      RegisterUsbManagerClient(usb_services[idx], device_manager_clients[idx]);
      service_worker_observers[idx]->WaitForWorkerStarted();
      EXPECT_EQ(device_removed_futures[idx].Get()->guid,
                fake_device_info->guid());
    }
  }

  usb_device_manager_.AddDevice(fake_device_info);

  // DeviceRemoved event when the service worker is running.
  {
    std::vector<TestFuture<device::mojom::UsbDeviceInfoPtr>>
        device_removed_futures(num_workers);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      auto& device_removed_future = device_removed_futures[idx];
      auto* version = context()->GetLiveVersion(version_ids[idx]);
      ASSERT_NE(version, nullptr);
      EXPECT_EQ(version->running_status(),
                blink::EmbeddedWorkerStatus::kRunning);
      EXPECT_CALL(device_manager_clients[idx], OnDeviceRemoved)
          .WillOnce(
              [&](auto d) { device_removed_future.SetValue(std::move(d)); });
    }
    DisconnectDevice(fake_device_info);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      EXPECT_EQ(device_removed_futures[idx].Get()->guid,
                fake_device_info->guid());
    }
  }
}

TEST_F(ServiceWorkerUsbDelegateObserverTest, OnDeviceManagerConnectionError) {
  size_t num_workers = 10;
  std::vector<GURL> origins;
  std::vector<scoped_refptr<ServiceWorkerRegistration>> registrations;
  std::vector<int64_t> version_ids;
  std::vector<mojo::Remote<blink::mojom::WebUsbService>> usb_services(
      num_workers);
  std::vector<MockDeviceManagerClient> device_manager_clients(num_workers);
  for (size_t idx = 0; idx < num_workers; ++idx) {
    origins.push_back(
        GURL(base::StringPrintf("https://www.example%zu.com", idx)));
    registrations.push_back(InstallServiceWorker(origins[idx]));
    auto* version = registrations[idx]->newest_installed_version();
    ASSERT_NE(version, nullptr);
    version_ids.push_back(version->version_id());
    StartServiceWorker(version);
    usb_services[idx] = CreateUsbService(version);
    RegisterUsbManagerClient(usb_services[idx], device_manager_clients[idx]);
  }

  for (size_t idx = 0; idx < num_workers; ++idx) {
    auto* version = context()->GetLiveVersion(version_ids[idx]);
    ASSERT_NE(version, nullptr);
    EXPECT_EQ(version->running_status(), blink::EmbeddedWorkerStatus::kRunning);
    auto services =
        context()->usb_delegate_observer()->GetUsbServicesForTesting(
            registrations[idx]->id());
    ASSERT_EQ(services.size(), 1u);
    EXPECT_EQ(services[0]->clients().size(), 1u);
  }
  usb_delegate().OnDeviceManagerConnectionError();
  for (size_t idx = 0; idx < num_workers; ++idx) {
    auto services =
        context()->usb_delegate_observer()->GetUsbServicesForTesting(
            registrations[idx]->id());
    ASSERT_EQ(services.size(), 1u);
    EXPECT_TRUE(services[0]->clients().empty());
  }
}

TEST_F(ServiceWorkerUsbDelegateObserverTest, OnPermissionRevoked) {
  device::MockUsbMojoDevice mock_device;
  auto fake_device_info = CreateFakeDevice();
  auto device_info = ConnectDevice(fake_device_info, &mock_device);

  size_t num_workers = 10;
  std::vector<GURL> origins;
  std::vector<scoped_refptr<ServiceWorkerRegistration>> registrations;
  std::vector<int64_t> version_ids;
  std::vector<mojo::Remote<blink::mojom::WebUsbService>> usb_services(
      num_workers);
  std::vector<MockDeviceManagerClient> device_manager_clients(num_workers);
  for (size_t idx = 0; idx < num_workers; ++idx) {
    origins.push_back(
        GURL(base::StringPrintf("https://www.example%zu.com", idx)));
    registrations.push_back(InstallServiceWorker(origins[idx]));
    auto* version = registrations[idx]->newest_installed_version();
    ASSERT_NE(version, nullptr);
    version_ids.push_back(version->version_id());
    StartServiceWorker(version);
    usb_services[idx] = CreateUsbService(version);
    RegisterUsbManagerClient(usb_services[idx], device_manager_clients[idx]);
  }

  for (size_t idx = 0; idx < num_workers; ++idx) {
    auto* version = registrations[idx]->GetNewestVersion();
    ASSERT_NE(version, nullptr);
    StartServiceWorker(version);
    EXPECT_EQ(version->running_status(), blink::EmbeddedWorkerStatus::kRunning);

    mojo::Remote<device::mojom::UsbDevice> device;
    usb_services[idx]->GetDevice(device_info->guid,
                                 device.BindNewPipeAndPassReceiver());
    EXPECT_CALL(mock_device, Open)
        .WillOnce(base::test::RunOnceCallback<0>(NewUsbOpenDeviceSuccess()));
    TestFuture<device::mojom::UsbOpenDeviceResultPtr> open_future;
    device->Open(open_future.GetCallback());
    EXPECT_TRUE(open_future.Get()->is_success());

    auto origin = url::Origin::Create(origins[idx]);
    base::RunLoop run_loop;
    EXPECT_CALL(usb_delegate(), GetDeviceInfo)
        .WillOnce(Return(device_info.get()));
    EXPECT_CALL(usb_delegate(),
                HasDevicePermission(_, nullptr, origin, Ref(*device_info)))
        .WillOnce(Return(false));
    EXPECT_CALL(mock_device, Close)
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    usb_delegate().OnPermissionRevoked(origin);
    run_loop.Run();

    testing::Mock::VerifyAndClearExpectations(&usb_delegate());
  }
}

TEST_F(ServiceWorkerUsbDelegateObserverTest,
       OnPermissionRevokedMultipleServicesForSameRegistration) {
  device::MockUsbMojoDevice mock_device1;
  device::MockUsbMojoDevice mock_device2;
  auto fake_device_info1 = CreateFakeDevice();
  auto fake_device_info2 = CreateFakeDevice();
  auto device_info1 = ConnectDevice(fake_device_info1, &mock_device1);
  auto device_info2 = ConnectDevice(fake_device_info2, &mock_device2);

  const GURL origin_url(kTestUrl);
  auto registration = InstallServiceWorker(origin_url);
  auto* version1 = registration->active_version();
  ASSERT_NE(version1, nullptr);
  StartServiceWorker(version1);
  auto usb_service1 = CreateUsbService(version1);

  // Create an installing version for the same registration to simulate a
  // service worker update where both versions are simultaneously running.
  auto version2 =
      CreateNewServiceWorkerVersion(context()->registry(), registration,
                                    GURL("https://www.google.com/worker2.js"),
                                    blink::mojom::ScriptType::kClassic);
  version2->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version2->SetStatus(ServiceWorkerVersion::Status::INSTALLING);
  registration->SetInstallingVersion(version2);
  StartServiceWorker(version2.get());
  auto usb_service2 = CreateUsbService(version2.get());

  EXPECT_EQ(context()
                ->usb_delegate_observer()
                ->GetUsbServicesForTesting(registration->id())
                .size(),
            2u);

  // Both versions open a USB device connection.
  mojo::Remote<device::mojom::UsbDevice> device1;
  usb_service1->GetDevice(device_info1->guid,
                          device1.BindNewPipeAndPassReceiver());
  EXPECT_CALL(mock_device1, Open)
      .WillOnce(base::test::RunOnceCallback<0>(NewUsbOpenDeviceSuccess()));
  TestFuture<device::mojom::UsbOpenDeviceResultPtr> open_future1;
  device1->Open(open_future1.GetCallback());
  EXPECT_TRUE(open_future1.Get()->is_success());

  mojo::Remote<device::mojom::UsbDevice> device2;
  usb_service2->GetDevice(device_info2->guid,
                          device2.BindNewPipeAndPassReceiver());
  EXPECT_CALL(mock_device2, Open)
      .WillOnce(base::test::RunOnceCallback<0>(NewUsbOpenDeviceSuccess()));
  TestFuture<device::mojom::UsbOpenDeviceResultPtr> open_future2;
  device2->Open(open_future2.GetCallback());
  EXPECT_TRUE(open_future2.Get()->is_success());

  // When permission is revoked for the origin, BOTH WebUsbServiceImpl
  // instances must be notified and close their open device connections.
  auto origin = url::Origin::Create(origin_url);
  TestFuture<void> close_future1;
  TestFuture<void> close_future2;
  EXPECT_CALL(usb_delegate(), GetDeviceInfo(_, device_info1->guid))
      .WillOnce(Return(device_info1.get()));
  EXPECT_CALL(usb_delegate(), GetDeviceInfo(_, device_info2->guid))
      .WillOnce(Return(device_info2.get()));
  EXPECT_CALL(usb_delegate(),
              HasDevicePermission(_, nullptr, origin, Ref(*device_info1)))
      .WillOnce(Return(false));
  EXPECT_CALL(usb_delegate(),
              HasDevicePermission(_, nullptr, origin, Ref(*device_info2)))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_device1, Close)
      .WillOnce(RunClosure(close_future1.GetRepeatingCallback()));
  EXPECT_CALL(mock_device2, Close)
      .WillOnce(RunClosure(close_future2.GetRepeatingCallback()));

  usb_delegate().OnPermissionRevoked(origin);
  EXPECT_TRUE(close_future1.Wait());
  EXPECT_TRUE(close_future2.Wait());
}

TEST_F(ServiceWorkerUsbDelegateObserverTest,
       RemovedFromUsbDelegateObserverWhenNoRegistration) {
  const GURL origin(kTestUrl);
  EXPECT_TRUE(usb_delegate().observer_list().empty());
  auto registration = InstallServiceWorker(origin);
  auto* version = registration->newest_installed_version();
  ASSERT_NE(version, nullptr);
  StartServiceWorker(version);
  auto usb_service = CreateUsbService(version);
  MockDeviceManagerClient device_manager_client;
  RegisterUsbManagerClient(usb_service, device_manager_client);
  auto services = context()->usb_delegate_observer()->GetUsbServicesForTesting(
      registration->id());
  ASSERT_EQ(services.size(), 1u);
  base::WeakPtr<WebUsbServiceImpl> service_impl = services[0];
  ASSERT_TRUE(service_impl);
  EXPECT_EQ(service_impl->clients().size(), 1u);
  EXPECT_FALSE(usb_delegate().observer_list().empty());

  TestFuture<void> connection_error_future;
  EXPECT_CALL(device_manager_client, ConnectionError())
      .WillOnce(RunClosure(connection_error_future.GetRepeatingCallback()));

  TestFuture<blink::ServiceWorkerStatusCode> unregister_future;
  context()->UnregisterServiceWorker(
      registration->scope(), registration->key(),
      /*is_immediate=*/true, ServiceWorkerRegistration::DeleteInitiator::kTest,
      unregister_future.GetCallback());
  EXPECT_EQ(unregister_future.Get<0>(), blink::ServiceWorkerStatusCode::kOk);
  EXPECT_TRUE(connection_error_future.Wait());
  // Wait until all of the
  // ServiceWorkerDeviceDelegateObserver::OnRegistrationDeleted are called.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return usb_delegate().observer_list().empty(); }));
  ASSERT_TRUE(service_impl);
  EXPECT_TRUE(service_impl->clients().empty());
}

TEST_F(ServiceWorkerUsbDelegateObserverTest,
       ActivatingWorkerNotDoubleNotified) {
  const GURL origin(kTestUrl);
  auto registration = InstallServiceWorker(origin);
  auto* version = registration->active_version();
  ASSERT_NE(version, nullptr);
  StartServiceWorker(version);
  version->SetStatus(ServiceWorkerVersion::Status::ACTIVATING);

  auto usb_service = CreateUsbService(version);
  MockDeviceManagerClient device_manager_client;
  RegisterUsbManagerClient(usb_service, device_manager_client);

  device::MockUsbMojoDevice mock_device;
  auto fake_device_info = CreateFakeDevice();
  TestFuture<device::mojom::UsbDeviceInfoPtr> device_added_future;
  EXPECT_CALL(device_manager_client, OnDeviceAdded)
      .Times(1)
      .WillOnce([&](auto d) { device_added_future.SetValue(std::move(d)); });

  ConnectDevice(fake_device_info, &mock_device);
  EXPECT_EQ(device_added_future.Get()->guid, fake_device_info->guid());

  // Transitioning from ACTIVATING to ACTIVATED should not notify a second time.
  version->SetStatus(ServiceWorkerVersion::Status::ACTIVATED);
  FlushUsbServicePipe(usb_service);
}

TEST_F(ServiceWorkerUsbDelegateObserverTest,
       RunningWorkerWithoutClientQueuesPendingCallback) {
  const GURL origin(kTestUrl);
  auto registration = InstallServiceWorker(origin);
  auto* version = registration->active_version();
  ASSERT_NE(version, nullptr);
  StartServiceWorker(version);
  EXPECT_EQ(version->running_status(), blink::EmbeddedWorkerStatus::kRunning);
  EXPECT_EQ(version->status(), ServiceWorkerVersion::Status::ACTIVATED);

  // Create WebUsbServiceImpl while the worker is kRunning, but do not register
  // the client yet (simulating SetClient Mojo call still in flight).
  auto usb_service = CreateUsbService(version);

  device::MockUsbMojoDevice mock_device;
  auto fake_device_info = CreateFakeDevice();
  ConnectDevice(fake_device_info, &mock_device);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return context()
        ->usb_delegate_observer()
        ->GetPendingCallbacksForTesting()
        .contains(version->version_id());
  }));

  // When SetClient completes, the queued event should be delivered.
  MockDeviceManagerClient device_manager_client;
  TestFuture<device::mojom::UsbDeviceInfoPtr> device_added_future;
  EXPECT_CALL(device_manager_client, OnDeviceAdded).WillOnce([&](auto d) {
    device_added_future.SetValue(std::move(d));
  });
  RegisterUsbManagerClient(usb_service, device_manager_client);
  EXPECT_EQ(device_added_future.Get()->guid, fake_device_info->guid());
  EXPECT_FALSE(context()
                   ->usb_delegate_observer()
                   ->GetPendingCallbacksForTesting()
                   .contains(version->version_id()));
}

TEST_F(ServiceWorkerUsbDelegateObserverTest,
       PrunesStoppedUsbServiceAfterServiceWorkerStopThenStart) {
  device::MockUsbMojoDevice mock_device;
  auto fake_device_info = CreateFakeDevice();
  auto device_info = ConnectDevice(fake_device_info, &mock_device);

  const GURL origin(kTestUrl);
  auto registration = InstallServiceWorker(origin);
  auto* version = registration->newest_installed_version();
  ASSERT_NE(version, nullptr);
  StartServiceWorker(version);
  auto usb_service = CreateUsbService(version);
  EXPECT_FALSE(context()
                   ->usb_delegate_observer()
                   ->GetUsbServicesForTesting(registration->id())
                   .empty());

  // Create a connection so that we can get to the point when the UsbService is
  // destroyed by expecting DecrementConnectionCount being called.
  mojo::Remote<device::mojom::UsbDevice> device;
  usb_service->GetDevice(device_info->guid,
                         device.BindNewPipeAndPassReceiver());
  EXPECT_CALL(mock_device, Open)
      .WillOnce(base::test::RunOnceCallback<0>(NewUsbOpenDeviceSuccess()));
  TestFuture<device::mojom::UsbOpenDeviceResultPtr> open_future;
  device->Open(open_future.GetCallback());
  EXPECT_TRUE(open_future.Get()->is_success());

  // Simulate the scenario of stopping the worker, the UsbService will be
  // destroyed.
  base::RunLoop run_loop;
  usb_service.set_disconnect_handler(run_loop.QuitClosure());
  EXPECT_CALL(
      usb_delegate(),
      DecrementConnectionCount(_, url::Origin::Create(origin)))  // never called
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  StopServiceWorker(version);
  usb_service.reset();
  run_loop.Run();
  EXPECT_TRUE(context()
                  ->usb_delegate_observer()
                  ->GetUsbServicesForTesting(registration->id())
                  .empty());

  // Then start the worker and create a new UsbService.
  StartServiceWorker(version);
  usb_service = CreateUsbService(version);
  EXPECT_EQ(context()
                ->usb_delegate_observer()
                ->GetUsbServicesForTesting(registration->id())
                .size(),
            1u);
}

TEST_F(ServiceWorkerUsbDelegateObserverTest,
       RestartBrowserWithInstalledServiceWorker) {
  const GURL origin(kTestUrl);
  auto registration = InstallServiceWorker(origin);
  auto registration_id = registration->id();
  auto* version = registration->newest_installed_version();
  ASSERT_NE(version, nullptr);
  auto version_id = version->version_id();
  EXPECT_TRUE(context()->GetLiveRegistration(registration_id));

  // Simulate a browser restart scenario where the registration_id_map of
  // ServiceWorkerUsbDelegateObserver is empty, and the
  // ServiceWorkerRegistration, ServiceWorkerVersion are not alive.
  registration.reset();
  EXPECT_FALSE(context()->GetLiveRegistration(registration_id));
  EXPECT_FALSE(context()->GetLiveVersion(version_id));
  context()->SetServiceWorkerUsbDelegateObserverForTesting(
      std::make_unique<ServiceWorkerUsbDelegateObserver>(context()));
  EXPECT_TRUE(
      context()->usb_delegate_observer()->registration_id_map().empty());

  // Create ServiceWorkerRegistration and ServiceWorkerVersion by finding the
  // registration.
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(origin));
  auto [status, found_registration] = FindRegistration(registration_id, key);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  EXPECT_TRUE(found_registration);
  version = found_registration->GetNewestVersion();
  EXPECT_NE(version, nullptr);
  EXPECT_TRUE(version->has_usb_event_handlers());
  EXPECT_TRUE(context()->GetLiveRegistration(registration_id));
  const auto it =
      context()->usb_delegate_observer()->registration_id_map().find(
          registration_id);
  EXPECT_NE(it,
            context()->usb_delegate_observer()->registration_id_map().end());
  EXPECT_EQ(it->second.key, key);
  EXPECT_TRUE(it->second.has_event_handlers);
}

TEST_F(ServiceWorkerUsbDelegateObserverTest, NoPermissionNotStartWorker) {
  const GURL origin(kTestUrl);
  auto registration = InstallServiceWorker(origin);
  auto* version = registration->newest_installed_version();
  ASSERT_NE(version, nullptr);

  device::MockUsbMojoDevice mock_device;
  auto fake_device_info = CreateFakeDevice();
  EXPECT_CALL(usb_delegate(), HasDevicePermission).WillOnce(Return(false));
  auto device_info = ConnectDevice(fake_device_info, &mock_device);
  EXPECT_EQ(version->running_status(), blink::EmbeddedWorkerStatus::kStopped);
}

TEST_F(ServiceWorkerUsbDelegateObserverTest, ProcessPendingCallback) {
  size_t num_workers = 10;
  std::vector<GURL> origins;
  std::vector<scoped_refptr<ServiceWorkerRegistration>> registrations;
  std::vector<int64_t> version_ids;
  std::vector<mojo::Remote<blink::mojom::WebUsbService>> usb_services(
      num_workers);
  std::vector<MockDeviceManagerClient> device_manager_clients(num_workers);
  for (size_t idx = 0; idx < num_workers; ++idx) {
    origins.push_back(
        GURL(base::StringPrintf("https://www.example%zu.com", idx)));
    registrations.push_back(InstallServiceWorker(origins[idx]));
    auto* version = registrations[idx]->newest_installed_version();
    ASSERT_NE(version, nullptr);
    version_ids.push_back(version->version_id());
  }

  device::MockUsbMojoDevice mock_device1;
  auto fake_device_info1 = CreateFakeDevice();
  auto fake_device_info2 = CreateFakeDevice();
  // DeviceAdded event when the service worker is not running.
  {
    std::vector<std::unique_ptr<TestServiceWorkerObserver>>
        service_worker_observers;
    std::vector<TestFuture<device::mojom::UsbDeviceInfoPtr>>
        device_added_futures(num_workers);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      service_worker_observers.push_back(
          std::make_unique<TestServiceWorkerObserver>(context()->wrapper(),
                                                      version_ids[idx]));
    }
    auto device_info1 = ConnectDevice(fake_device_info1, &mock_device1);
    const auto& pending_callbacks =
        context()->usb_delegate_observer()->GetPendingCallbacksForTesting();
    for (size_t idx = 0; idx < num_workers; ++idx) {
      service_worker_observers[idx]->WaitForWorkerStarting();
      auto* version = context()->GetLiveVersion(version_ids[idx]);
      ASSERT_NE(version, nullptr);
      // Not to register UsbManagerClient until later to have callback stored
      // and be consumed later.
      usb_services[idx] = CreateUsbService(version);
      service_worker_observers[idx]->WaitForWorkerStarted();
      const auto it = pending_callbacks.find(version_ids[idx]);
      EXPECT_NE(it, pending_callbacks.end());
      EXPECT_EQ(it->second.size(), 1u);
    }

    for (size_t idx = 0; idx < num_workers; ++idx) {
      auto& device_added_future = device_added_futures[idx];
      EXPECT_CALL(device_manager_clients[idx], OnDeviceAdded)
          .WillOnce(
              [&](auto d) { device_added_future.SetValue(std::move(d)); });
      RegisterUsbManagerClient(usb_services[idx], device_manager_clients[idx]);
      EXPECT_EQ(device_added_futures[idx].Get()->guid,
                fake_device_info1->guid());
      EXPECT_FALSE(pending_callbacks.contains(version_ids[idx]));
    }
  }
}

TEST_F(ServiceWorkerUsbDelegateObserverTest,
       ClearPendingCallbackWhenWorkerStopped) {
  size_t num_workers = 10;
  std::vector<GURL> origins;
  std::vector<scoped_refptr<ServiceWorkerRegistration>> registrations;
  std::vector<int64_t> version_ids;
  for (size_t idx = 0; idx < num_workers; ++idx) {
    origins.push_back(
        GURL(base::StringPrintf("https://www.example%zu.com", idx)));
    registrations.push_back(InstallServiceWorker(origins[idx]));
    auto* version = registrations[idx]->newest_installed_version();
    ASSERT_NE(version, nullptr);
    version_ids.push_back(version->version_id());
  }

  device::MockUsbMojoDevice mock_device1;
  auto fake_device_info1 = CreateFakeDevice();
  auto fake_device_info2 = CreateFakeDevice();

  std::vector<mojo::Remote<blink::mojom::WebUsbService>> usb_services(
      num_workers);
  std::vector<MockDeviceManagerClient> device_manager_clients(num_workers);
  // DeviceAdded event when the service worker is not running.
  {
    std::vector<std::unique_ptr<TestServiceWorkerObserver>>
        service_worker_observers;
    std::vector<TestFuture<device::mojom::UsbDeviceInfoPtr>>
        device_added_futures(num_workers);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      service_worker_observers.push_back(
          std::make_unique<TestServiceWorkerObserver>(context()->wrapper(),
                                                      version_ids[idx]));
    }
    auto device_info1 = ConnectDevice(fake_device_info1, &mock_device1);
    const auto& pending_callbacks =
        context()->usb_delegate_observer()->GetPendingCallbacksForTesting();
    for (size_t idx = 0; idx < num_workers; ++idx) {
      service_worker_observers[idx]->WaitForWorkerStarting();
      auto* version = context()->GetLiveVersion(version_ids[idx]);
      ASSERT_NE(version, nullptr);
      usb_services[idx] = CreateUsbService(version);
      service_worker_observers[idx]->WaitForWorkerStarted();
      const auto it = pending_callbacks.find(version_ids[idx]);
      EXPECT_NE(it, pending_callbacks.end());
      EXPECT_EQ(it->second.size(), 1u);
    }

    for (size_t idx = 0; idx < num_workers; ++idx) {
      auto* version = context()->GetLiveVersion(version_ids[idx]);
      ASSERT_NE(version, nullptr);
      ASSERT_EQ(version->version_id(), version_ids[idx]);
      StopServiceWorker(version);
      service_worker_observers[idx]->WaitForWorkerStopped();
      // Returning from `WaitForWorkerStopped()` does not guarantee that all of
      // the `ServiceWorkerContextCoreObserver` are called. It might be the case
      // that `TestServiceWorkerObserver::OnStopped` is called but
      // `ServiceWorkerDeviceDelegateObserver::OnStopped` is not called yet. To
      // handle this case, start the worker and then check the state when the
      // work is started because that is the point at which all of the
      // `ServiceWorkerContextCoreObservers::OnStopped` have been called.
      StartServiceWorker(version);
      EXPECT_FALSE(pending_callbacks.contains(version_ids[idx]));
    }
  }
}

TEST_F(ServiceWorkerUsbDelegateObserverNoEventHandlersTest,
       DeviceAddedNotStartWorker) {
  const GURL origin(kTestUrl);
  auto registration = InstallServiceWorker(origin);
  auto* version = registration->newest_installed_version();
  ASSERT_TRUE(version);

  device::MockUsbMojoDevice mock_device;
  auto fake_device_info = CreateFakeDevice();
  auto device_info = ConnectDevice(fake_device_info, &mock_device);
  EXPECT_EQ(version->running_status(), blink::EmbeddedWorkerStatus::kStopped);
}

TEST_F(ServiceWorkerUsbDelegateObserverNoEventHandlersTest,
       RestartBrowserWithInstalledServiceWorker) {
  const GURL origin(kTestUrl);
  auto registration = InstallServiceWorker(origin);
  auto registration_id = registration->id();
  auto* version = registration->newest_installed_version();
  ASSERT_NE(version, nullptr);
  auto version_id = version->version_id();
  EXPECT_TRUE(context()->GetLiveRegistration(registration_id));

  // Simulate a browser restart scenario where the registration_id_map of
  // ServiceWorkerUsbDelegateObserver is empty, and the
  // ServiceWorkerRegistration, ServiceWorkerVersion are not alive.
  registration.reset();
  EXPECT_FALSE(context()->GetLiveRegistration(registration_id));
  EXPECT_FALSE(context()->GetLiveVersion(version_id));
  context()->SetServiceWorkerUsbDelegateObserverForTesting(
      std::make_unique<ServiceWorkerUsbDelegateObserver>(context()));
  EXPECT_TRUE(
      context()->usb_delegate_observer()->registration_id_map().empty());

  // Create ServiceWorkerRegistration and ServiceWorkerVersion by finding the
  // registration.
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(origin));
  auto [status, found_registration] = FindRegistration(registration_id, key);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  EXPECT_TRUE(found_registration);
  version = found_registration->GetNewestVersion();
  EXPECT_NE(version, nullptr);
  EXPECT_FALSE(version->has_usb_event_handlers());
  EXPECT_TRUE(context()->GetLiveRegistration(registration_id));
  EXPECT_TRUE(
      context()->usb_delegate_observer()->registration_id_map().empty());

  StartServiceWorker(version);
  EXPECT_FALSE(version->has_usb_event_handlers());
  EXPECT_TRUE(
      context()->usb_delegate_observer()->registration_id_map().empty());
}

TEST_F(ServiceWorkerUsbDelegateObserverNoEventHandlersTest,
       DeviceRemovedClosesOpenDeviceConnection) {
  device::MockUsbMojoDevice mock_device;
  auto fake_device_info = CreateFakeDevice();
  auto device_info = ConnectDevice(fake_device_info, &mock_device);

  const GURL origin(kTestUrl);
  auto registration = InstallServiceWorker(origin);
  auto* version = registration->newest_installed_version();
  ASSERT_NE(version, nullptr);
  StartServiceWorker(version);
  auto usb_service = CreateUsbService(version);

  mojo::Remote<device::mojom::UsbDevice> device;
  usb_service->GetDevice(device_info->guid,
                         device.BindNewPipeAndPassReceiver());
  EXPECT_CALL(mock_device, Open)
      .WillOnce(base::test::RunOnceCallback<0>(NewUsbOpenDeviceSuccess()));
  TestFuture<device::mojom::UsbOpenDeviceResultPtr> open_future;
  device->Open(open_future.GetCallback());
  EXPECT_TRUE(open_future.Get()->is_success());

  TestFuture<void> disconnect_future;
  EXPECT_CALL(mock_device, Close)
      .WillOnce(RunClosure(disconnect_future.GetRepeatingCallback()));
  DisconnectDevice(fake_device_info);
  EXPECT_TRUE(disconnect_future.Wait());
}

// Shutdown the service worker context and make sure that
// ServiceWorkerUsbDelegateObserver removes itself from the usb delegate
// properly.
TEST_F(ServiceWorkerUsbDelegateObserverTest, ShutdownServiceWorkerContext) {
  const GURL origin(kTestUrl);
  auto registration = InstallServiceWorker(origin);
  auto* version = registration->newest_installed_version();
  ASSERT_NE(version, nullptr);
  StartServiceWorker(version);
  auto usb_service = CreateUsbService(version);
  EXPECT_FALSE(context()
                   ->usb_delegate_observer()
                   ->GetUsbServicesForTesting(registration->id())
                   .empty());

  EXPECT_FALSE(usb_delegate().observer_list().empty());
  helper()->ShutdownContext();
  EXPECT_TRUE(usb_delegate().observer_list().empty());
}

}  // namespace content
