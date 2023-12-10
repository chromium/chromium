// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_device_delegate_observer_unittest.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_usb_delegate_observer.h"
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
                 int process_id,
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
    registry()->FindRegistrationForId(registration_id, key,
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
    EXPECT_EQ(context()
                  ->usb_delegate_observer()
                  ->GetUsbServiceForTesting(registrations[idx]->id())
                  ->clients()
                  .size(),
              1u);
  }
  usb_delegate().OnDeviceManagerConnectionError();
  for (size_t idx = 0; idx < num_workers; ++idx) {
    EXPECT_TRUE(context()
                    ->usb_delegate_observer()
                    ->GetUsbServiceForTesting(registrations[idx]->id())
                    ->clients()
                    .empty());
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
       RemovedFromUsbDelegateObserverWhenNoRegistration) {
  const GURL origin(kTestUrl);
  EXPECT_TRUE(usb_delegate().observer_list().empty());
  auto registration = InstallServiceWorker(origin);
  auto* version = registration->newest_installed_version();
  ASSERT_NE(version, nullptr);
  StartServiceWorker(version);
  auto usb_service = CreateUsbService(version);
  EXPECT_FALSE(usb_delegate().observer_list().empty());

  TestFuture<blink::ServiceWorkerStatusCode> unregister_future;
  context()->UnregisterServiceWorker(registration->scope(), registration->key(),
                                     /*is_immediate=*/true,
                                     unregister_future.GetCallback());
  EXPECT_EQ(unregister_future.Get<0>(), blink::ServiceWorkerStatusCode::kOk);
  // Wait until all of the
  // ServiceWorkerDeviceDelegateObserver::OnRegistrationDeleted are called.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(usb_delegate().observer_list().empty());
}

TEST_F(ServiceWorkerUsbDelegateObserverTest,
       HasLatestUsbServiceAfterServiceWorkerStopThenStart) {
  device::MockUsbMojoDevice mock_device;
  auto fake_device_info = CreateFakeDevice();
  auto device_info = ConnectDevice(fake_device_info, &mock_device);

  const GURL origin(kTestUrl);
  auto registration = InstallServiceWorker(origin);
  auto* version = registration->newest_installed_version();
  ASSERT_NE(version, nullptr);
  StartServiceWorker(version);
  auto usb_service = CreateUsbService(version);
  EXPECT_TRUE(context()->usb_delegate_observer()->GetUsbServiceForTesting(
      registration->id()));

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
  EXPECT_FALSE(context()->usb_delegate_observer()->GetUsbServiceForTesting(
      registration->id()));

  // Then start the worker and create a new UsbService.
  StartServiceWorker(version);
  usb_service = CreateUsbService(version);
  EXPECT_TRUE(context()->usb_delegate_observer()->GetUsbServiceForTesting(
      registration->id()));
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

// Shutdown the service worker context and make sure that
// ServiceWorkerUsbDelegateObserver removes itself from the usb delegate
// properly.
TEST_F(ServiceWorkerUsbDelegateObserverTest, ShutdownServiceWorkerContext) {
  const GURL origin(kTestUrl);
  auto registration = InstallServiceWorker(origin);
  auto* version = registration->newest_installed_version();
  ASSERT_NE(version, nullptr);
  StartServiceWorker(version);
  CreateUsbService(version);
  EXPECT_TRUE(context()->usb_delegate_observer()->GetUsbServiceForTesting(
      registration->id()));

  EXPECT_FALSE(usb_delegate().observer_list().empty());
  helper()->ShutdownContext();
  EXPECT_TRUE(usb_delegate().observer_list().empty());
}

}  // namespace content
