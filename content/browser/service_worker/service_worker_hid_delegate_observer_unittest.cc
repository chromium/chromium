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
#include "content/browser/hid/hid_service.h"
#include "content/browser/hid/hid_test_utils.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_device_delegate_observer_unittest.h"
#include "content/browser/service_worker/service_worker_hid_delegate_observer.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/hid_delegate.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/public/cpp/test/fake_hid_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

using ::base::test::RunClosure;
using ::base::test::TestFuture;
using ::testing::_;
using ::testing::Ref;
using ::testing::Return;

const char kTestUrl[] = "https://www.google.com";
const char kTestGuid[] = "test-guid";

class FakeHidConnectionClient : public device::mojom::HidConnectionClient {
 public:
  FakeHidConnectionClient() = default;
  FakeHidConnectionClient(FakeHidConnectionClient&) = delete;
  FakeHidConnectionClient& operator=(FakeHidConnectionClient&) = delete;
  ~FakeHidConnectionClient() override = default;

  void Bind(
      mojo::PendingReceiver<device::mojom::HidConnectionClient> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  // mojom::HidConnectionClient:
  void OnInputReport(uint8_t report_id,
                     const std::vector<uint8_t>& buffer) override {}

 private:
  mojo::Receiver<device::mojom::HidConnectionClient> receiver_{this};
};

class MockHidManagerClient : public device::mojom::HidManagerClient {
 public:
  MockHidManagerClient() = default;
  MockHidManagerClient(MockHidManagerClient&) = delete;
  MockHidManagerClient& operator=(MockHidManagerClient&) = delete;
  ~MockHidManagerClient() override = default;

  void Bind(mojo::PendingAssociatedReceiver<device::mojom::HidManagerClient>
                receiver) {
    receiver_.Bind(std::move(receiver));
  }

  MOCK_METHOD1(DeviceAdded, void(device::mojom::HidDeviceInfoPtr device_info));
  MOCK_METHOD1(DeviceRemoved,
               void(device::mojom::HidDeviceInfoPtr device_info));
  MOCK_METHOD1(DeviceChanged,
               void(device::mojom::HidDeviceInfoPtr device_info));

 private:
  mojo::AssociatedReceiver<device::mojom::HidManagerClient> receiver_{this};
};

class ServiceWorkerHidDelegateObserverTest
    : public ServiceWorkerDeviceDelegateObserverTest {
 public:
  ServiceWorkerHidDelegateObserverTest() = default;
  ServiceWorkerHidDelegateObserverTest(ServiceWorkerHidDelegateObserverTest&) =
      delete;
  ServiceWorkerHidDelegateObserverTest& operator=(
      ServiceWorkerHidDelegateObserverTest&) = delete;
  ~ServiceWorkerHidDelegateObserverTest() override = default;

  void SetUp() override {
    ServiceWorkerDeviceDelegateObserverTest::SetUp();
    ON_CALL(hid_delegate(), GetHidManager).WillByDefault(Return(&hid_manager_));
    ON_CALL(hid_delegate(), IsFidoAllowedForOrigin)
        .WillByDefault(Return(false));
    ON_CALL(hid_delegate(), HasDevicePermission).WillByDefault(Return(true));
  }

  void RegisterHidManagerClient(
      const mojo::Remote<blink::mojom::HidService>& service,
      MockHidManagerClient& hid_manager_client) {
    mojo::PendingAssociatedRemote<device::mojom::HidManagerClient>
        hid_manager_client_remote;
    hid_manager_client.Bind(
        hid_manager_client_remote.InitWithNewEndpointAndPassReceiver());
    service->RegisterClient(std::move(hid_manager_client_remote));
    FlushHidServicePipe(service);
  }

  void ConnectDevice(const device::mojom::HidDeviceInfo& device) {
    hid_manager_.AddDevice(device.Clone());
    hid_delegate().OnDeviceAdded(device);
  }

  void DisconnectDevice(const device::mojom::HidDeviceInfo& device) {
    hid_manager_.RemoveDevice(device.guid);
    hid_delegate().OnDeviceRemoved(device);
  }

  void UpdateDevice(const device::mojom::HidDeviceInfo& device) {
    hid_manager_.ChangeDevice(device.Clone());
    hid_delegate().OnDeviceChanged(device);
  }

  // Open a connection to |device|.
  mojo::Remote<device::mojom::HidConnection> OpenDevice(
      const mojo::Remote<blink::mojom::HidService>& hid_service,
      const device::mojom::HidDeviceInfoPtr& device,
      FakeHidConnectionClient& connection_client) {
    mojo::PendingRemote<device::mojom::HidConnectionClient>
        hid_connection_client;
    connection_client.Bind(
        hid_connection_client.InitWithNewPipeAndPassReceiver());
    TestFuture<mojo::PendingRemote<device::mojom::HidConnection>>
        pending_remote_future;
    EXPECT_CALL(hid_delegate(), GetDeviceInfo).WillOnce(Return(device.get()));
    EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
    EXPECT_CALL(hid_delegate(), IncrementConnectionCount);
    EXPECT_CALL(hid_delegate(), GetHidManager).WillOnce(Return(&hid_manager()));
    hid_service->Connect(device->guid, std::move(hid_connection_client),
                         pending_remote_future.GetCallback());

    mojo::Remote<device::mojom::HidConnection> connection;
    connection.Bind(pending_remote_future.Take());
    EXPECT_TRUE(connection);
    testing::Mock::VerifyAndClearExpectations(&hid_delegate());
    return connection;
  }

  device::mojom::HidDeviceInfoPtr CreateDeviceWithNoReports(
      const std::string& guid = kTestGuid) {
    auto collection = device::mojom::HidCollectionInfo::New();
    collection->usage = device::mojom::HidUsageAndPage::New(1, 1);
    auto device_info = device::mojom::HidDeviceInfo::New();
    device_info->guid = guid;
    device_info->collections.push_back(std::move(collection));
    return device_info;
  }

  device::mojom::HidDeviceInfoPtr CreateDeviceWithOneReport(
      const std::string& guid = kTestGuid) {
    auto device_info = CreateDeviceWithNoReports(guid);
    auto collection = device::mojom::HidCollectionInfo::New();
    collection->usage = device::mojom::HidUsageAndPage::New(2, 2);
    collection->input_reports.push_back(
        device::mojom::HidReportDescription::New());
    device_info->collections.push_back(std::move(collection));
    return device_info;
  }

  device::mojom::HidDeviceInfoPtr CreateDeviceWithTwoReports(
      const std::string& guid = kTestGuid) {
    auto device_info = CreateDeviceWithOneReport(guid);
    auto collection = device::mojom::HidCollectionInfo::New();
    collection->usage = device::mojom::HidUsageAndPage::New(3, 3);
    collection->output_reports.push_back(
        device::mojom::HidReportDescription::New());
    device_info->collections.push_back(std::move(collection));
    return device_info;
  }

  mojo::Remote<blink::mojom::HidService> CreateHidService(
      ServiceWorkerVersion* version) {
    auto const& origin = version->key().origin();
    mojo::Remote<blink::mojom::HidService> service;
    auto* embedded_worker = version->GetEmbeddedWorkerForTesting();
    EXPECT_CALL(hid_delegate(), IsServiceWorkerAllowedForOrigin(origin))
        .WillOnce(Return(true));
    embedded_worker->BindHidService(origin,
                                    service.BindNewPipeAndPassReceiver());
    return service;
  }

  void FlushHidServicePipe(
      const mojo::Remote<blink::mojom::HidService>& hid_service) {
    // Run GetDevices to flush mojo request.
    TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>> devices_future;
    hid_service->GetDevices(devices_future.GetCallback());
    EXPECT_TRUE(devices_future.Wait());
  }

  MockHidDelegate& hid_delegate() { return test_client_.delegate(); }

  FakeHidConnectionClient* connection_client() { return &connection_client_; }

  device::FakeHidManager& hid_manager() { return hid_manager_; }

  MockHidManagerClient& hid_manager_client() { return hid_manager_client_; }

 protected:
  MockHidManagerClient hid_manager_client_;
  HidTestContentBrowserClient test_client_;
  device::FakeHidManager hid_manager_;
  FakeHidConnectionClient connection_client_;
  ScopedContentBrowserClientSetting setting{&test_client_};
};

}  // namespace

TEST_F(ServiceWorkerHidDelegateObserverTest, DeviceAdded) {
  size_t num_workers = 10;
  std::vector<const GURL> origins;
  std::vector<scoped_refptr<ServiceWorkerRegistration>> registrations;
  std::vector<int64_t> version_ids;
  std::vector<mojo::Remote<blink::mojom::HidService>> hid_services(num_workers);
  std::vector<MockHidManagerClient> hid_manager_clients(num_workers);
  for (size_t idx = 0; idx < num_workers; ++idx) {
    origins.push_back(
        GURL(base::StringPrintf("https://www.example%zu.com", idx)));
    registrations.push_back(InstallServiceWorker(origins[idx]));
    auto* version = registrations[idx]->newest_installed_version();
    ASSERT_NE(version, nullptr);
    version_ids.push_back(version->version_id());
    StartServiceWorker(version);
    hid_services[idx] = CreateHidService(version);
    RegisterHidManagerClient(hid_services[idx], hid_manager_clients[idx]);
  }

  auto device = CreateDeviceWithOneReport("device-guid");
  // DeviceAdded event when the service worker is running.
  {
    std::vector<TestFuture<device::mojom::HidDeviceInfoPtr>>
        device_added_futures(num_workers);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      auto& device_added_future = device_added_futures[idx];
      auto* version = context()->GetLiveVersion(version_ids[idx]);
      ASSERT_NE(version, nullptr);
      EXPECT_EQ(version->running_status(), EmbeddedWorkerStatus::RUNNING);
      EXPECT_CALL(hid_manager_clients[idx], DeviceAdded).WillOnce([&](auto d) {
        device_added_future.SetValue(std::move(d));
      });
    }
    ConnectDevice(*device);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      EXPECT_EQ(device_added_futures[idx].Get()->guid, device->guid);
    }
  }
}

TEST_F(ServiceWorkerHidDelegateObserverTest, DeviceRemoved) {
  size_t num_workers = 10;
  std::vector<const GURL> origins;
  std::vector<scoped_refptr<ServiceWorkerRegistration>> registrations;
  std::vector<int64_t> version_ids;
  std::vector<mojo::Remote<blink::mojom::HidService>> hid_services(num_workers);
  std::vector<MockHidManagerClient> hid_manager_clients(num_workers);
  for (size_t idx = 0; idx < num_workers; ++idx) {
    origins.push_back(
        GURL(base::StringPrintf("https://www.example%zu.com", idx)));
    registrations.push_back(InstallServiceWorker(origins[idx]));
    auto* version = registrations[idx]->newest_installed_version();
    ASSERT_NE(version, nullptr);
    version_ids.push_back(version->version_id());
    StartServiceWorker(version);
    hid_services[idx] = CreateHidService(version);
    RegisterHidManagerClient(hid_services[idx], hid_manager_clients[idx]);
  }

  auto device = CreateDeviceWithOneReport();
  hid_manager_.AddDevice(device.Clone());
  // DeviceRemoved event when the service wokrer is running.
  {
    std::vector<TestFuture<device::mojom::HidDeviceInfoPtr>>
        device_removed_futures(num_workers);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      auto& device_removed_future = device_removed_futures[idx];
      auto* version = context()->GetLiveVersion(version_ids[idx]);
      ASSERT_NE(version, nullptr);
      EXPECT_EQ(version->running_status(), EmbeddedWorkerStatus::RUNNING);
      EXPECT_CALL(hid_manager_clients[idx], DeviceRemoved)
          .WillOnce(
              [&](auto d) { device_removed_future.SetValue(std::move(d)); });
    }
    DisconnectDevice(*device);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      EXPECT_EQ(device_removed_futures[idx].Get()->guid, device->guid);
    }
  }
}

TEST_F(ServiceWorkerHidDelegateObserverTest, DeviceChanged) {
  size_t num_workers = 10;
  std::vector<const GURL> origins;
  std::vector<scoped_refptr<ServiceWorkerRegistration>> registrations;
  std::vector<int64_t> version_ids;
  std::vector<mojo::Remote<blink::mojom::HidService>> hid_services(num_workers);
  std::vector<MockHidManagerClient> hid_manager_clients(num_workers);
  for (size_t idx = 0; idx < num_workers; ++idx) {
    origins.push_back(
        GURL(base::StringPrintf("https://www.example%zu.com", idx)));
    registrations.push_back(InstallServiceWorker(origins[idx]));
    auto* version = registrations[idx]->newest_installed_version();
    ASSERT_NE(version, nullptr);
    version_ids.push_back(version->version_id());
    StartServiceWorker(version);
    hid_services[idx] = CreateHidService(version);
    RegisterHidManagerClient(hid_services[idx], hid_manager_clients[idx]);
  }

  auto device = CreateDeviceWithOneReport();
  hid_manager_.AddDevice(device.Clone());
  // DeviceChanged event when the service wokrer is running.
  {
    std::vector<TestFuture<device::mojom::HidDeviceInfoPtr>>
        device_changed_futures(num_workers);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      auto& device_changed_future = device_changed_futures[idx];
      auto* version = context()->GetLiveVersion(version_ids[idx]);
      ASSERT_NE(version, nullptr);
      EXPECT_EQ(version->running_status(), EmbeddedWorkerStatus::RUNNING);
      EXPECT_CALL(hid_manager_clients[idx], DeviceChanged)
          .WillOnce(
              [&](auto d) { device_changed_future.SetValue(std::move(d)); });
    }
    device = CreateDeviceWithOneReport();
    UpdateDevice(*device);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      EXPECT_EQ(device_changed_futures[idx].Get()->guid, device->guid);
    }
  }
}

TEST_F(ServiceWorkerHidDelegateObserverTest, OnHidManagerConnectionError) {
  size_t num_workers = 10;
  std::vector<const GURL> origins;
  std::vector<scoped_refptr<ServiceWorkerRegistration>> registrations;
  std::vector<int64_t> version_ids;
  std::vector<mojo::Remote<blink::mojom::HidService>> hid_services(num_workers);
  std::vector<MockHidManagerClient> hid_manager_clients(num_workers);
  for (size_t idx = 0; idx < num_workers; ++idx) {
    origins.push_back(
        GURL(base::StringPrintf("https://www.example%zu.com", idx)));
    registrations.push_back(InstallServiceWorker(origins[idx]));
    auto* version = registrations[idx]->newest_installed_version();
    ASSERT_NE(version, nullptr);
    version_ids.push_back(version->version_id());
    StartServiceWorker(version);
    hid_services[idx] = CreateHidService(version);
    RegisterHidManagerClient(hid_services[idx], hid_manager_clients[idx]);
  }

  for (size_t idx = 0; idx < num_workers; ++idx) {
    auto* version = context()->GetLiveVersion(version_ids[idx]);
    ASSERT_NE(version, nullptr);
    EXPECT_EQ(version->running_status(), EmbeddedWorkerStatus::RUNNING);
    EXPECT_EQ(context()
                  ->hid_delegate_observer()
                  ->GetHidServiceForTesting(registrations[idx]->id())
                  ->clients()
                  .size(),
              1u);
  }
  hid_delegate().OnHidManagerConnectionError();
  for (size_t idx = 0; idx < num_workers; ++idx) {
    EXPECT_TRUE(context()
                    ->hid_delegate_observer()
                    ->GetHidServiceForTesting(registrations[idx]->id())
                    ->clients()
                    .empty());
  }
}

TEST_F(ServiceWorkerHidDelegateObserverTest, OnPermissionRevoked) {
  auto device = CreateDeviceWithOneReport();
  ConnectDevice(*device);

  size_t num_workers = 10;
  std::vector<const GURL> origins;
  std::vector<scoped_refptr<ServiceWorkerRegistration>> registrations;
  std::vector<int64_t> version_ids;
  std::vector<mojo::Remote<blink::mojom::HidService>> hid_services(num_workers);
  std::vector<MockHidManagerClient> hid_manager_clients(num_workers);
  for (size_t idx = 0; idx < num_workers; ++idx) {
    origins.push_back(
        GURL(base::StringPrintf("https://www.example%zu.com", idx)));
    registrations.push_back(InstallServiceWorker(origins[idx]));
    auto* version = registrations[idx]->newest_installed_version();
    ASSERT_NE(version, nullptr);
    version_ids.push_back(version->version_id());
    StartServiceWorker(version);
    hid_services[idx] = CreateHidService(version);
    RegisterHidManagerClient(hid_services[idx], hid_manager_clients[idx]);
  }

  std::vector<FakeHidConnectionClient> hid_connection_clients(num_workers);
  std::vector<mojo::Remote<device::mojom::HidConnection>> hid_connections(
      num_workers);
  for (size_t idx = 0; idx < num_workers; ++idx) {
    auto* version = registrations[idx]->GetNewestVersion();
    ASSERT_NE(version, nullptr);
    StartServiceWorker(version);
    EXPECT_EQ(version->running_status(), EmbeddedWorkerStatus::RUNNING);
    hid_connections[idx] =
        OpenDevice(hid_services[idx], device, hid_connection_clients[idx]);
    EXPECT_FALSE(context()
                     ->hid_delegate_observer()
                     ->GetHidServiceForTesting(registrations[idx]->id())
                     ->GetWatchersForTesting()
                     .empty());

    base::RunLoop run_loop;
    auto origin = url::Origin::Create(origins[idx]);
    EXPECT_CALL(hid_delegate(), GetDeviceInfo).WillOnce(Return(device.get()));
    EXPECT_CALL(hid_delegate(), HasDevicePermission(_, origin, Ref(*device)))
        .WillOnce(Return(false));
    EXPECT_CALL(hid_delegate(), DecrementConnectionCount(_, origin))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    hid_delegate().OnPermissionRevoked(origin);
    run_loop.Run();
    EXPECT_TRUE(context()
                    ->hid_delegate_observer()
                    ->GetHidServiceForTesting(registrations[idx]->id())
                    ->GetWatchersForTesting()
                    .empty());
    testing::Mock::VerifyAndClearExpectations(&hid_delegate());
  }
}

TEST_F(ServiceWorkerHidDelegateObserverTest,
       RemovedFromHidDelegateObserverWhenNoRegistration) {
  const GURL origin(kTestUrl);
  EXPECT_TRUE(hid_delegate().observer_list().empty());
  auto registration = InstallServiceWorker(origin);
  auto* version = registration->newest_installed_version();
  ASSERT_NE(version, nullptr);
  StartServiceWorker(version);
  auto hid_service = CreateHidService(version);
  EXPECT_FALSE(hid_delegate().observer_list().empty());

  TestFuture<blink::ServiceWorkerStatusCode> unregister_future;
  context()->UnregisterServiceWorker(registration->scope(), registration->key(),
                                     /*is_immediate=*/true,
                                     unregister_future.GetCallback());
  EXPECT_EQ(unregister_future.Get<0>(), blink::ServiceWorkerStatusCode::kOk);
  // Wait until all of the
  // ServiceWorkerDeviceDelegateObserver::OnRegistrationDeleted are called.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(hid_delegate().observer_list().empty());
}

TEST_F(ServiceWorkerHidDelegateObserverTest,
       HasLatestHidServiceAfterServiceWorkerStopThenStart) {
  auto device = CreateDeviceWithOneReport();
  ConnectDevice(*device);

  const GURL origin(kTestUrl);
  auto registration = InstallServiceWorker(origin);
  auto* version = registration->newest_installed_version();
  ASSERT_NE(version, nullptr);
  StartServiceWorker(version);
  auto hid_service = CreateHidService(version);
  EXPECT_TRUE(context()->hid_delegate_observer()->GetHidServiceForTesting(
      registration->id()));

  // Create a connection so that we can get to the point when the HidService is
  // destroyed by expecting DecrementConnectionCount being called.
  FakeHidConnectionClient hid_connection_client;
  auto hid_connection = OpenDevice(hid_service, device, hid_connection_client);
  EXPECT_FALSE(context()
                   ->hid_delegate_observer()
                   ->GetHidServiceForTesting(registration->id())
                   ->GetWatchersForTesting()
                   .empty());

  // Simulate the scenario of stopping the worker, the HidService will be
  // destroyed.
  base::RunLoop run_loop;
  hid_service.set_disconnect_handler(run_loop.QuitClosure());
  EXPECT_CALL(hid_delegate(),
              DecrementConnectionCount(_, url::Origin::Create(origin)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  StopServiceWorker(version);
  hid_service.reset();
  run_loop.Run();
  EXPECT_FALSE(context()->hid_delegate_observer()->GetHidServiceForTesting(
      registration->id()));

  // Then start the worker and create a new HidService.
  StartServiceWorker(version);
  hid_service = CreateHidService(version);
  EXPECT_TRUE(context()->hid_delegate_observer()->GetHidServiceForTesting(
      registration->id()));
}

}  // namespace content
