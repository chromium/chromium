// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
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

  MOCK_METHOD(void, DeviceAdded, (device::mojom::HidDeviceInfoPtr), (override));
  MOCK_METHOD(void,
              DeviceRemoved,
              (device::mojom::HidDeviceInfoPtr),
              (override));
  MOCK_METHOD(void,
              DeviceChanged,
              (device::mojom::HidDeviceInfoPtr),
              (override));

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
    hid_delegate().SetAssertBrowserContext(true);
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
    auto* embedded_worker = version->embedded_worker();
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
    // This simulates the scenario where the service worker script has an HID
    // event handler.
    version->set_has_hid_event_handlers(true);
  }

 protected:
  MockHidManagerClient hid_manager_client_;
  HidTestContentBrowserClient test_client_;
  device::FakeHidManager hid_manager_;
  FakeHidConnectionClient connection_client_;
  ScopedContentBrowserClientSetting setting{&test_client_};
};

class ServiceWorkerHidDelegateObserverNoEventHandlersTest
    : public ServiceWorkerHidDelegateObserverTest {
 public:
  void ServiceWorkerInstalling(
      scoped_refptr<ServiceWorkerVersion> version) override {
    // Do nothing to simluate no HID event handlers.
  }
};

}  // namespace

TEST_F(ServiceWorkerHidDelegateObserverTest, DeviceAdded) {
  size_t num_workers = 10;
  std::vector<GURL> origins;
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
  }

  auto device1 = CreateDeviceWithOneReport("device1-guid");
  auto device2 = CreateDeviceWithOneReport("device2-guid");
  // DeviceAdded event when the service worker is not running.
  {
    std::vector<std::unique_ptr<TestServiceWorkerObserver>>
        service_worker_observers;
    std::vector<TestFuture<device::mojom::HidDeviceInfoPtr>>
        device_added_futures(num_workers);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      service_worker_observers.push_back(
          std::make_unique<TestServiceWorkerObserver>(context()->wrapper(),
                                                      version_ids[idx]));
      auto& device_added_future = device_added_futures[idx];
      EXPECT_CALL(hid_manager_clients[idx], DeviceAdded).WillOnce([&](auto d) {
        device_added_future.SetValue(std::move(d));
      });
    }
    ConnectDevice(*device1);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      service_worker_observers[idx]->WaitForWorkerStarting();
      auto* version = context()->GetLiveVersion(version_ids[idx]);
      ASSERT_NE(version, nullptr);
      hid_services[idx] = CreateHidService(version);
      RegisterHidManagerClient(hid_services[idx], hid_manager_clients[idx]);
      service_worker_observers[idx]->WaitForWorkerStarted();
      EXPECT_EQ(device_added_futures[idx].Get()->guid, device1->guid);
    }
  }

  // DeviceAdded event when the service worker is running.
  {
    std::vector<TestFuture<device::mojom::HidDeviceInfoPtr>>
        device_added_futures(num_workers);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      auto& device_added_future = device_added_futures[idx];
      auto* version = context()->GetLiveVersion(version_ids[idx]);
      ASSERT_NE(version, nullptr);
      EXPECT_EQ(version->running_status(),
                blink::EmbeddedWorkerStatus::kRunning);
      EXPECT_CALL(hid_manager_clients[idx], DeviceAdded).WillOnce([&](auto d) {
        device_added_future.SetValue(std::move(d));
      });
    }
    ConnectDevice(*device2);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      EXPECT_EQ(device_added_futures[idx].Get()->guid, device2->guid);
    }
  }
}

TEST_F(ServiceWorkerHidDelegateObserverTest, DeviceRemoved) {
  size_t num_workers = 10;
  std::vector<GURL> origins;
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
  }

  auto device = CreateDeviceWithOneReport();
  hid_manager_.AddDevice(device.Clone());
  // DeviceRemoved event when the service worker is not running.
  {
    std::vector<std::unique_ptr<TestServiceWorkerObserver>>
        service_worker_observers;
    std::vector<TestFuture<device::mojom::HidDeviceInfoPtr>>
        device_removed_futures(num_workers);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      service_worker_observers.push_back(
          std::make_unique<TestServiceWorkerObserver>(context()->wrapper(),
                                                      version_ids[idx]));
      auto& device_removed_future = device_removed_futures[idx];
      EXPECT_CALL(hid_manager_clients[idx], DeviceRemoved)
          .WillOnce(
              [&](auto d) { device_removed_future.SetValue(std::move(d)); });
    }
    DisconnectDevice(*device);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      service_worker_observers[idx]->WaitForWorkerStarting();
      auto* version = context()->GetLiveVersion(version_ids[idx]);
      ASSERT_NE(version, nullptr);
      hid_services[idx] = CreateHidService(version);
      RegisterHidManagerClient(hid_services[idx], hid_manager_clients[idx]);
      service_worker_observers[idx]->WaitForWorkerStarted();
      EXPECT_EQ(device_removed_futures[idx].Get()->guid, device->guid);
    }
  }

  hid_manager_.AddDevice(device.Clone());
  // DeviceRemoved event when the service worker is running.
  {
    std::vector<TestFuture<device::mojom::HidDeviceInfoPtr>>
        device_removed_futures(num_workers);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      auto& device_removed_future = device_removed_futures[idx];
      auto* version = context()->GetLiveVersion(version_ids[idx]);
      ASSERT_NE(version, nullptr);
      EXPECT_EQ(version->running_status(),
                blink::EmbeddedWorkerStatus::kRunning);
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
  std::vector<GURL> origins;
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
  }

  // DeviceChanged event when the service worker is not running.
  auto device = CreateDeviceWithOneReport();
  hid_manager_.AddDevice(device.Clone());
  {
    std::vector<std::unique_ptr<TestServiceWorkerObserver>>
        service_worker_observers;
    std::vector<TestFuture<device::mojom::HidDeviceInfoPtr>>
        device_changed_futures(num_workers);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      service_worker_observers.push_back(
          std::make_unique<TestServiceWorkerObserver>(context()->wrapper(),
                                                      version_ids[idx]));
      auto& device_changed_future = device_changed_futures[idx];
      EXPECT_CALL(hid_manager_clients[idx], DeviceChanged)
          .WillOnce(
              [&](auto d) { device_changed_future.SetValue(std::move(d)); });
    }
    device = CreateDeviceWithTwoReports();
    UpdateDevice(*device);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      service_worker_observers[idx]->WaitForWorkerStarting();
      auto* version = context()->GetLiveVersion(version_ids[idx]);
      ASSERT_NE(version, nullptr);
      hid_services[idx] = CreateHidService(version);
      RegisterHidManagerClient(hid_services[idx], hid_manager_clients[idx]);
      service_worker_observers[idx]->WaitForWorkerStarted();
      EXPECT_EQ(device_changed_futures[idx].Get()->guid, device->guid);
    }
  }

  // DeviceChanged event when the service worker is running.
  {
    std::vector<TestFuture<device::mojom::HidDeviceInfoPtr>>
        device_changed_futures(num_workers);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      auto& device_changed_future = device_changed_futures[idx];
      auto* version = context()->GetLiveVersion(version_ids[idx]);
      ASSERT_NE(version, nullptr);
      EXPECT_EQ(version->running_status(),
                blink::EmbeddedWorkerStatus::kRunning);
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
  std::vector<GURL> origins;
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
    EXPECT_EQ(version->running_status(), blink::EmbeddedWorkerStatus::kRunning);
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
  std::vector<GURL> origins;
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
    EXPECT_EQ(version->running_status(), blink::EmbeddedWorkerStatus::kRunning);
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
    EXPECT_CALL(hid_delegate(),
                HasDevicePermission(_, nullptr, origin, Ref(*device)))
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

TEST_F(ServiceWorkerHidDelegateObserverTest,
       RestartBrowserWithInstalledServiceWorker) {
  const GURL origin(kTestUrl);
  auto registration = InstallServiceWorker(origin);
  auto registration_id = registration->id();
  auto* version = registration->newest_installed_version();
  ASSERT_NE(version, nullptr);
  auto version_id = version->version_id();
  EXPECT_TRUE(context()->GetLiveRegistration(registration_id));

  // Simulate a browser restart scenario where the registration_id_map of
  // ServiceWorkerHidDelegateObserver is empty, and the
  // ServiceWorkerRegistration, ServiceWorkerVersion are not alive.
  registration.reset();
  EXPECT_FALSE(context()->GetLiveRegistration(registration_id));
  EXPECT_FALSE(context()->GetLiveVersion(version_id));
  context()->SetServiceWorkerHidDelegateObserverForTesting(
      std::make_unique<ServiceWorkerHidDelegateObserver>(context()));
  EXPECT_TRUE(
      context()->hid_delegate_observer()->registration_id_map().empty());

  // Create ServiceWorkerRegistration and ServiceWorkerVersion by finding the
  // registration.
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(origin));
  auto [status, found_registration] = FindRegistration(registration_id, key);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  EXPECT_TRUE(found_registration);
  version = found_registration->GetNewestVersion();
  EXPECT_NE(version, nullptr);
  EXPECT_TRUE(version->has_hid_event_handlers());
  EXPECT_TRUE(context()->GetLiveRegistration(registration_id));
  const auto it =
      context()->hid_delegate_observer()->registration_id_map().find(
          registration_id);
  EXPECT_NE(it,
            context()->hid_delegate_observer()->registration_id_map().end());
  EXPECT_EQ(it->second.key, key);
  EXPECT_TRUE(it->second.has_event_handlers);
}

TEST_F(ServiceWorkerHidDelegateObserverTest, NoPermissionNotStartWorker) {
  const GURL origin(kTestUrl);
  auto registration = InstallServiceWorker(origin);
  auto* version = registration->newest_installed_version();
  ASSERT_NE(version, nullptr);

  auto device = CreateDeviceWithOneReport();
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(false));
  ConnectDevice(*device);
  EXPECT_EQ(version->running_status(), blink::EmbeddedWorkerStatus::kStopped);
}

TEST_F(ServiceWorkerHidDelegateObserverTest, NoReportsDeviceNotStartWorker) {
  const GURL origin(kTestUrl);
  auto registration = InstallServiceWorker(origin);
  auto* version = registration->newest_installed_version();
  ASSERT_NE(version, nullptr);

  auto device = CreateDeviceWithNoReports();
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  ConnectDevice(*device);
  EXPECT_EQ(version->running_status(), blink::EmbeddedWorkerStatus::kStopped);
}

TEST_F(ServiceWorkerHidDelegateObserverTest, ProcessPendingCallback) {
  size_t num_workers = 10;
  std::vector<GURL> origins;
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
  }

  auto device1 = CreateDeviceWithOneReport("device1-guid");
  auto device2 = CreateDeviceWithOneReport("device2-guid");
  // DeviceAdded event when the service worker is not running.
  {
    std::vector<std::unique_ptr<TestServiceWorkerObserver>>
        service_worker_observers;
    std::vector<TestFuture<device::mojom::HidDeviceInfoPtr>>
        device_added_futures(num_workers);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      service_worker_observers.push_back(
          std::make_unique<TestServiceWorkerObserver>(context()->wrapper(),
                                                      version_ids[idx]));
    }
    ConnectDevice(*device1);
    const auto& pending_callbacks =
        context()->hid_delegate_observer()->GetPendingCallbacksForTesting();
    for (size_t idx = 0; idx < num_workers; ++idx) {
      service_worker_observers[idx]->WaitForWorkerStarting();
      auto* version = context()->GetLiveVersion(version_ids[idx]);
      ASSERT_NE(version, nullptr);
      // Not to register HidManagerClient until later to have callback stored
      // and be consumed later.
      hid_services[idx] = CreateHidService(version);
      service_worker_observers[idx]->WaitForWorkerStarted();
      const auto it = pending_callbacks.find(version_ids[idx]);
      EXPECT_NE(it, pending_callbacks.end());
      EXPECT_EQ(it->second.size(), 1u);
    }

    for (size_t idx = 0; idx < num_workers; ++idx) {
      auto& device_added_future = device_added_futures[idx];
      EXPECT_CALL(hid_manager_clients[idx], DeviceAdded).WillOnce([&](auto d) {
        device_added_future.SetValue(std::move(d));
      });
      RegisterHidManagerClient(hid_services[idx], hid_manager_clients[idx]);
      EXPECT_EQ(device_added_futures[idx].Get()->guid, device1->guid);
      EXPECT_FALSE(pending_callbacks.contains(version_ids[idx]));
    }
  }
}

TEST_F(ServiceWorkerHidDelegateObserverTest,
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

  auto device1 = CreateDeviceWithOneReport("device1-guid");
  auto device2 = CreateDeviceWithOneReport("device2-guid");
  std::vector<mojo::Remote<blink::mojom::HidService>> hid_services(num_workers);
  std::vector<MockHidManagerClient> hid_manager_clients(num_workers);
  // DeviceAdded event when the service worker is not running.
  {
    std::vector<std::unique_ptr<TestServiceWorkerObserver>>
        service_worker_observers;
    std::vector<TestFuture<device::mojom::HidDeviceInfoPtr>>
        device_added_futures(num_workers);
    for (size_t idx = 0; idx < num_workers; ++idx) {
      service_worker_observers.push_back(
          std::make_unique<TestServiceWorkerObserver>(context()->wrapper(),
                                                      version_ids[idx]));
    }
    ConnectDevice(*device1);
    const auto& pending_callbacks =
        context()->hid_delegate_observer()->GetPendingCallbacksForTesting();
    for (size_t idx = 0; idx < num_workers; ++idx) {
      service_worker_observers[idx]->WaitForWorkerStarting();
      auto* version = context()->GetLiveVersion(version_ids[idx]);
      ASSERT_NE(version, nullptr);
      hid_services[idx] = CreateHidService(version);
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

TEST_F(ServiceWorkerHidDelegateObserverNoEventHandlersTest,
       DeviceAddedNotStartWorker) {
  const GURL origin(kTestUrl);
  auto registration = InstallServiceWorker(origin);
  auto* version = registration->newest_installed_version();
  ASSERT_TRUE(version);

  auto device = CreateDeviceWithOneReport();
  ConnectDevice(*device);
  EXPECT_EQ(version->running_status(), blink::EmbeddedWorkerStatus::kStopped);
}

TEST_F(ServiceWorkerHidDelegateObserverNoEventHandlersTest,
       RestartBrowserWithInstalledServiceWorker) {
  const GURL origin(kTestUrl);
  auto registration = InstallServiceWorker(origin);
  auto registration_id = registration->id();
  auto* version = registration->newest_installed_version();
  ASSERT_NE(version, nullptr);
  auto version_id = version->version_id();
  EXPECT_TRUE(context()->GetLiveRegistration(registration_id));

  // Simulate a browser restart scenario where the registration_id_map of
  // ServiceWorkerHidDelegateObserver is empty, and the
  // ServiceWorkerRegistration, ServiceWorkerVersion are not alive.
  registration.reset();
  EXPECT_FALSE(context()->GetLiveRegistration(registration_id));
  EXPECT_FALSE(context()->GetLiveVersion(version_id));
  context()->SetServiceWorkerHidDelegateObserverForTesting(
      std::make_unique<ServiceWorkerHidDelegateObserver>(context()));
  EXPECT_TRUE(
      context()->hid_delegate_observer()->registration_id_map().empty());

  // Create ServiceWorkerRegistration and ServiceWorkerVersion by finding the
  // registration.
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(origin));
  auto [status, found_registration] = FindRegistration(registration_id, key);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  EXPECT_TRUE(found_registration);
  version = found_registration->GetNewestVersion();
  EXPECT_NE(version, nullptr);
  EXPECT_FALSE(version->has_hid_event_handlers());
  EXPECT_TRUE(context()->GetLiveRegistration(registration_id));
  EXPECT_TRUE(
      context()->hid_delegate_observer()->registration_id_map().empty());

  StartServiceWorker(version);
  EXPECT_FALSE(version->has_hid_event_handlers());
  EXPECT_TRUE(
      context()->hid_delegate_observer()->registration_id_map().empty());
}

// Shutdown the service worker context and make sure that
// ServiceWorkerHidDelegateObserver removes itself from the hid delegate
// properly.
TEST_F(ServiceWorkerHidDelegateObserverTest, ShutdownServiceWorkerContext) {
  const GURL origin(kTestUrl);
  auto registration = InstallServiceWorker(origin);
  auto* version = registration->newest_installed_version();
  ASSERT_NE(version, nullptr);
  StartServiceWorker(version);
  CreateHidService(version);
  EXPECT_TRUE(context()->hid_delegate_observer()->GetHidServiceForTesting(
      registration->id()));

  EXPECT_FALSE(hid_delegate().observer_list().empty());
  helper()->ShutdownContext();
  EXPECT_TRUE(hid_delegate().observer_list().empty());
}

}  // namespace content
