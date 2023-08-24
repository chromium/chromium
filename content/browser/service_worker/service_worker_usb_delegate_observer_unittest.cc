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

}  // namespace

TEST_F(ServiceWorkerUsbDelegateObserverTest, OnDeviceAdded) {
  size_t num_workers = 10;
  std::vector<const GURL> origins;
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

  mojo::Remote<device::mojom::UsbDevice> device;
  device::MockUsbMojoDevice mock_device;
  auto fake_device_info = CreateFakeDevice();

  // DeviceAdded event when the service worker is running.
  {
    std::vector<TestFuture<device::mojom::UsbDeviceInfoPtr>>
        device_added_futures(num_workers);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      auto& device_added_future = device_added_futures[idx];
      auto* version = context()->GetLiveVersion(version_ids[idx]);
      ASSERT_NE(version, nullptr);
      EXPECT_EQ(version->running_status(), EmbeddedWorkerStatus::RUNNING);
      EXPECT_CALL(device_manager_clients[idx], OnDeviceAdded)
          .WillOnce(
              [&](auto d) { device_added_future.SetValue(std::move(d)); });
    }
    auto device_info = ConnectDevice(fake_device_info, &mock_device);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      EXPECT_EQ(device_added_futures[idx].Get()->guid, device_info->guid);
    }
  }
}

TEST_F(ServiceWorkerUsbDelegateObserverTest, OnDeviceRemoved) {
  size_t num_workers = 10;
  std::vector<const GURL> origins;
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

  auto fake_device_info = CreateFakeDevice();
  usb_device_manager_.AddDevice(fake_device_info);
  // DeviceRemoved event when the service worker is running.
  {
    std::vector<TestFuture<device::mojom::UsbDeviceInfoPtr>>
        device_removed_futures(num_workers);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      auto& device_removed_future = device_removed_futures[idx];
      auto* version = context()->GetLiveVersion(version_ids[idx]);
      ASSERT_NE(version, nullptr);
      EXPECT_EQ(version->running_status(), EmbeddedWorkerStatus::RUNNING);
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
  std::vector<const GURL> origins;
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
    EXPECT_EQ(version->running_status(), EmbeddedWorkerStatus::RUNNING);
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
  std::vector<const GURL> origins;
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
    EXPECT_EQ(version->running_status(), EmbeddedWorkerStatus::RUNNING);

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
                HasDevicePermission(_, origin, Ref(*device_info)))
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

}  // namespace content
