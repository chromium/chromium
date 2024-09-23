// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <memory>
#include <vector>

#include "base/barrier_closure.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "content/browser/hid/hid_service.h"
#include "content/browser/hid/hid_test_utils.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/hid_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/device/public/cpp/test/fake_hid_manager.h"
#include "services/device/public/cpp/test/hid_test_util.h"
#include "services/device/public/cpp/test/test_report_descriptors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"

namespace content {

namespace {

using ::base::test::RunClosure;
using ::base::test::TestFuture;
using ::testing::_;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Expectation;
using ::testing::Invoke;
using ::testing::Return;

enum HidServiceCreationType {
  kCreateUsingRenderFrameHost,
  kCreateUsingServiceWorkerContextCore,
};

const char kTestUrl[] = "https://www.google.com";
const char kTestGuid[] = "test-guid";
const char kCrossOriginTestUrl[] = "https://www.chromium.org";

std::string HidServiceCreationTypeToString(HidServiceCreationType type) {
  switch (type) {
    case kCreateUsingRenderFrameHost:
      return "CreateUsingRenderFrameHost";
    case kCreateUsingServiceWorkerContextCore:
      return "CreateUsingServiceWorkerContextCore";
  }
}

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

class HidServiceTestHelper {
 public:
  HidServiceTestHelper() {
    ON_CALL(hid_delegate(), GetHidManager).WillByDefault(Return(&hid_manager_));
    ON_CALL(hid_delegate(), IsFidoAllowedForOrigin)
        .WillByDefault(Return(false));
  }
  HidServiceTestHelper(HidServiceTestHelper&) = delete;
  HidServiceTestHelper& operator=(HidServiceTestHelper&) = delete;
  ~HidServiceTestHelper() = default;

  void ConnectDevice(const device::mojom::HidDeviceInfo& device) {
    hid_manager_.AddDevice(device.Clone());
    hid_delegate().OnDeviceAdded(device);
  }

  void DisconnectDevice(const device::mojom::HidDeviceInfo& device) {
    hid_manager_.RemoveDevice(device.guid);
    hid_delegate().OnDeviceRemoved(device);
  }

  // Open a connection to |device|.
  mojo::Remote<device::mojom::HidConnection> OpenDevice(
      const mojo::Remote<blink::mojom::HidService>& hid_service,
      device::mojom::HidDeviceInfoPtr& device,
      FakeHidConnectionClient& connection_client) {
    mojo::PendingRemote<device::mojom::HidConnectionClient>
        hid_connection_client;
    connection_client.Bind(
        hid_connection_client.InitWithNewPipeAndPassReceiver());
    TestFuture<mojo::PendingRemote<device::mojom::HidConnection>>
        pending_remote_future;
    hid_service->Connect(device->guid, std::move(hid_connection_client),
                         pending_remote_future.GetCallback());
    mojo::Remote<device::mojom::HidConnection> connection;
    connection.Bind(pending_remote_future.Take());
    EXPECT_TRUE(connection);
    return connection;
  }

  void UpdateDevice(const device::mojom::HidDeviceInfo& device) {
    hid_manager_.ChangeDevice(device.Clone());
    hid_delegate().OnDeviceChanged(device);
  }

  device::mojom::HidDeviceInfoPtr CreateDeviceWithNoReports() {
    auto collection = device::mojom::HidCollectionInfo::New();
    collection->usage = device::mojom::HidUsageAndPage::New(1, 1);
    auto device_info = device::mojom::HidDeviceInfo::New();
    device_info->guid = kTestGuid;
    device_info->collections.push_back(std::move(collection));
    return device_info;
  }

  device::mojom::HidDeviceInfoPtr CreateDeviceWithOneReport() {
    auto device_info = CreateDeviceWithNoReports();
    auto collection = device::mojom::HidCollectionInfo::New();
    collection->usage = device::mojom::HidUsageAndPage::New(2, 2);
    collection->input_reports.push_back(
        device::mojom::HidReportDescription::New());
    device_info->collections.push_back(std::move(collection));
    return device_info;
  }

  device::mojom::HidDeviceInfoPtr CreateDeviceWithTwoReports() {
    auto device_info = CreateDeviceWithOneReport();
    auto collection = device::mojom::HidCollectionInfo::New();
    collection->usage = device::mojom::HidUsageAndPage::New(3, 3);
    collection->output_reports.push_back(
        device::mojom::HidReportDescription::New());
    device_info->collections.push_back(std::move(collection));
    return device_info;
  }

  device::mojom::HidDeviceInfoPtr CreateFidoDevice() {
    return device::CreateDeviceFromReportDescriptor(
        /*vendor_id=*/0x1234, /*product_id=*/0xabcd,
        device::TestReportDescriptors::FidoU2fHid());
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

 private:
  HidTestContentBrowserClient test_client_;
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;
  device::FakeHidManager hid_manager_;
  FakeHidConnectionClient connection_client_;
  ScopedContentBrowserClientSetting setting{&test_client_};
};

class HidServiceBaseTest : public testing::Test, public HidServiceTestHelper {
 public:
  HidServiceBaseTest() = default;
  HidServiceBaseTest(HidServiceBaseTest&) = delete;
  HidServiceBaseTest& operator=(HidServiceBaseTest&) = delete;
  ~HidServiceBaseTest() override = default;

  mojo::Remote<blink::mojom::HidService>& GetService(
      HidServiceCreationType type) {
    switch (type) {
      case kCreateUsingRenderFrameHost:
        web_contents_ =
            web_contents_factory_.CreateWebContents(&browser_context_);
        static_cast<TestWebContents*>(web_contents_)
            ->NavigateAndCommit(GURL(kTestUrl));
        static_cast<TestWebContents*>(web_contents_)
            ->GetPrimaryMainFrame()
            ->GetHidService(service_.BindNewPipeAndPassReceiver());
        break;
      case kCreateUsingServiceWorkerContextCore: {
        auto scope = GURL(kTestUrl);
        auto origin = url::Origin::Create(scope);
        auto worker_url = scope.Resolve("worker.js");
        embedded_worker_test_helper_ =
            std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());
        EmbeddedWorkerTestHelper::RegistrationAndVersionPair pair =
            embedded_worker_test_helper_->PrepareRegistrationAndVersion(
                scope, worker_url);
        worker_version_ = pair.second;
        worker_version_->set_fetch_handler_type(
            ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
        // Since this test fixture is used expecting device events being
        // handled, simulate the script having hid event handlers by setting
        // `has_hid_event_handlers_` of `worker_version_` before it is being
        // activated.
        worker_version_->set_has_hid_event_handlers(true);
        worker_version_->SetStatus(ServiceWorkerVersion::Status::ACTIVATED);
        pair.first->SetActiveVersion(worker_version_);
        auto* embedded_worker = worker_version_->embedded_worker();
        embedded_worker_test_helper_->StartWorker(
            embedded_worker,
            embedded_worker_test_helper_->CreateStartParams(pair.second));
        EXPECT_CALL(hid_delegate(), IsServiceWorkerAllowedForOrigin(origin))
            .WillOnce(Return(true));
        embedded_worker->BindHidService(origin,
                                        service_.BindNewPipeAndPassReceiver());
        break;
      }
    }
    RegisterHidManagerClient(service_);
    return service_;
  }

  BrowserContext* GetBrowserContext(HidServiceCreationType type) {
    switch (type) {
      case kCreateUsingRenderFrameHost:
        return &browser_context_;
      case kCreateUsingServiceWorkerContextCore:
        if (embedded_worker_test_helper_->context()) {
          return embedded_worker_test_helper_->context()
              ->wrapper()
              ->browser_context();
        }
        break;
    }
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  void CheckHidServiceConnectedState(HidServiceCreationType type,
                                     bool expected_state) {
    if (type == kCreateUsingRenderFrameHost) {
      ASSERT_EQ(web_contents_->IsConnectedToHidDevice(), expected_state);
    } else if (type == kCreateUsingServiceWorkerContextCore) {
      ASSERT_EQ(worker_version_->GetExternalRequestCountForTest(),
                expected_state ? 1u : 0u);
    }
  }

  MockHidManagerClient& hid_manager_client() { return hid_manager_client_; }

  void RegisterHidManagerClient(
      const mojo::Remote<blink::mojom::HidService>& service) {
    mojo::PendingAssociatedRemote<device::mojom::HidManagerClient>
        hid_manager_client;
    hid_manager_client_.Bind(
        hid_manager_client.InitWithNewEndpointAndPassReceiver());
    service->RegisterClient(std::move(hid_manager_client));
    FlushHidServicePipe(service);
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  mojo::Remote<blink::mojom::HidService> service_;

  // For create hid service using RenderFrameHost.
  TestBrowserContext browser_context_;
  TestWebContentsFactory web_contents_factory_;
  raw_ptr<WebContents> web_contents_;  // Owned by |web_contents_factory_|.
  MockHidManagerClient hid_manager_client_;

  // For create hid service using service worker.
  std::unique_ptr<EmbeddedWorkerTestHelper> embedded_worker_test_helper_;
  scoped_refptr<content::ServiceWorkerVersion> worker_version_;
};

class HidServiceRenderFrameHostTest : public RenderViewHostImplTestHarness,
                                      public HidServiceTestHelper {};

class HidServiceTest
    : public HidServiceBaseTest,
      public testing::WithParamInterface<HidServiceCreationType> {};

// Test fixture parameterized for fido allowed or not.
class HidServiceFidoTest : public HidServiceBaseTest,
                           public testing::WithParamInterface<
                               std::tuple<HidServiceCreationType, bool>> {};

// Test fixture for service worker specific tests.
class HidServiceServiceWorkerBrowserContextDestroyedTest
    : public HidServiceBaseTest {
 public:
  void DestroyBrowserContext() {
    // Reset |embedded_worker_test_helper_| will subsequently destroy the
    // BrowserContext associated with it.
    embedded_worker_test_helper_.reset();
  }

  void SetUp() override { GetService(kCreateUsingServiceWorkerContextCore); }
};

}  // namespace

TEST_P(HidServiceTest, GetDevicesWithPermission) {
  const auto& service = GetService(GetParam());

  auto collection = device::mojom::HidCollectionInfo::New();
  collection->usage = device::mojom::HidUsageAndPage::New(0xff00, 0x0001);
  collection->input_reports.push_back(
      device::mojom::HidReportDescription::New());
  auto device_info = device::mojom::HidDeviceInfo::New();
  device_info->guid = kTestGuid;
  device_info->collections.push_back(std::move(collection));
  ConnectDevice(*device_info);

  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));

  base::RunLoop run_loop;
  std::vector<device::mojom::HidDeviceInfoPtr> devices;
  service->GetDevices(base::BindLambdaForTesting(
      [&run_loop, &devices](std::vector<device::mojom::HidDeviceInfoPtr> d) {
        devices = std::move(d);
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(1u, devices.size());
}

TEST_P(HidServiceTest, GetDevicesWithoutPermission) {
  const auto& service = GetService(GetParam());

  auto device_info = CreateDeviceWithOneReport();
  ConnectDevice(*device_info);

  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(false));

  base::RunLoop run_loop;
  std::vector<device::mojom::HidDeviceInfoPtr> devices;
  service->GetDevices(base::BindLambdaForTesting(
      [&run_loop, &devices](std::vector<device::mojom::HidDeviceInfoPtr> d) {
        devices = std::move(d);
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(0u, devices.size());
}

TEST_P(HidServiceTest, RequestDevice) {
  auto service_creation_type = GetParam();
  const auto& service = GetService(service_creation_type);

  auto device_info = CreateDeviceWithOneReport();
  std::vector<device::mojom::HidDeviceInfoPtr> device_infos;
  device_infos.push_back(device_info.Clone());
  ConnectDevice(*device_info);

  if (service_creation_type == kCreateUsingRenderFrameHost) {
    EXPECT_CALL(hid_delegate(), CanRequestDevicePermission)
        .WillOnce(Return(true));
    EXPECT_CALL(hid_delegate(), RunChooserInternal)
        .WillOnce(Return(ByMove(std::move(device_infos))));
  }

  base::RunLoop run_loop;
  std::vector<device::mojom::HidDeviceInfoPtr> chosen_devices;
  service->RequestDevice(
      std::vector<blink::mojom::HidDeviceFilterPtr>(),
      std::vector<blink::mojom::HidDeviceFilterPtr>(),
      base::BindLambdaForTesting(
          [&run_loop,
           &chosen_devices](std::vector<device::mojom::HidDeviceInfoPtr> d) {
            chosen_devices = std::move(d);
            run_loop.Quit();
          }));
  run_loop.Run();
  if (service_creation_type == kCreateUsingRenderFrameHost) {
    EXPECT_EQ(1u, chosen_devices.size());
  } else {
    EXPECT_EQ(0u, chosen_devices.size());
  }
}

TEST_P(HidServiceTest, OpenAndCloseHidConnection) {
  auto service_creation_type = GetParam();
  const auto& service = GetService(service_creation_type);

  auto device_info = CreateDeviceWithOneReport();
  ConnectDevice(*device_info);

  mojo::PendingRemote<device::mojom::HidConnectionClient> hid_connection_client;
  connection_client()->Bind(
      hid_connection_client.InitWithNewPipeAndPassReceiver());

  CheckHidServiceConnectedState(service_creation_type, false);

  base::RunLoop run_loop;
  mojo::Remote<device::mojom::HidConnection> connection;
  EXPECT_CALL(hid_delegate(), GetDeviceInfo)
      .WillOnce(Return(device_info.get()));
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  EXPECT_CALL(hid_delegate(),
              IncrementConnectionCount(GetBrowserContext(service_creation_type),
                                       url::Origin::Create(GURL(kTestUrl))));
  service->Connect(
      kTestGuid, std::move(hid_connection_client),
      base::BindLambdaForTesting(
          [&run_loop,
           &connection](mojo::PendingRemote<device::mojom::HidConnection> c) {
            connection.Bind(std::move(c));
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_TRUE(connection.is_connected());

  CheckHidServiceConnectedState(service_creation_type, true);

  base::RunLoop disconnect_run_loop;
  // Destroying |connection| will also disconnect the watcher.
  EXPECT_CALL(hid_delegate(),
              DecrementConnectionCount(GetBrowserContext(service_creation_type),
                                       url::Origin::Create(GURL(kTestUrl))))
      .WillOnce(RunClosure(disconnect_run_loop.QuitClosure()));
  connection.reset();

  disconnect_run_loop.Run();
  CheckHidServiceConnectedState(service_creation_type, false);
}

TEST_P(HidServiceTest, OpenHidConnectionFail) {
  auto service_creation_type = GetParam();
  const auto& service = GetService(service_creation_type);

  // Note here no device is connected to the HID manager so opening connection
  // will fail.
  auto device_info = CreateDeviceWithOneReport();

  mojo::PendingRemote<device::mojom::HidConnectionClient> hid_connection_client;
  connection_client()->Bind(
      hid_connection_client.InitWithNewPipeAndPassReceiver());

  CheckHidServiceConnectedState(service_creation_type, false);

  base::RunLoop run_loop;
  mojo::Remote<device::mojom::HidConnection> connection;
  TestFuture<mojo::PendingRemote<device::mojom::HidConnection>>
      pending_remote_future;
  EXPECT_CALL(hid_delegate(), GetDeviceInfo)
      .WillOnce(Return(device_info.get()));
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  EXPECT_CALL(hid_delegate(),
              IncrementConnectionCount(GetBrowserContext(service_creation_type),
                                       url::Origin::Create(GURL(kTestUrl))));
  EXPECT_CALL(hid_delegate(),
              DecrementConnectionCount(GetBrowserContext(service_creation_type),
                                       url::Origin::Create(GURL(kTestUrl))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  service->Connect(kTestGuid, std::move(hid_connection_client),
                   pending_remote_future.GetCallback());
  EXPECT_FALSE(pending_remote_future.Take());

  run_loop.Run();
  CheckHidServiceConnectedState(service_creation_type, false);
}

TEST_F(HidServiceRenderFrameHostTest, OpenAndNavigateCrossOrigin) {
  // The test assumes the previous page gets deleted after navigation,
  // disconnecting the device. Disable back/forward cache to ensure that it
  // doesn't get preserved in the cache.
  // TODO(crbug.com/40232335): Integrate WebHID with bfcache and remove this.
  DisableBackForwardCacheForTesting(web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::HidService> service;
  contents()->GetPrimaryMainFrame()->GetHidService(
      service.BindNewPipeAndPassReceiver());

  auto device_info = CreateDeviceWithOneReport();
  ConnectDevice(*device_info);

  mojo::PendingRemote<device::mojom::HidConnectionClient> hid_connection_client;
  connection_client()->Bind(
      hid_connection_client.InitWithNewPipeAndPassReceiver());

  EXPECT_FALSE(contents()->IsConnectedToHidDevice());

  base::RunLoop run_loop;
  mojo::Remote<device::mojom::HidConnection> connection;
  EXPECT_CALL(hid_delegate(), GetDeviceInfo)
      .WillOnce(Return(device_info.get()));
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  EXPECT_CALL(hid_delegate(),
              IncrementConnectionCount(browser_context(),
                                       url::Origin::Create(GURL(kTestUrl))));
  service->Connect(
      kTestGuid, std::move(hid_connection_client),
      base::BindLambdaForTesting(
          [&run_loop,
           &connection](mojo::PendingRemote<device::mojom::HidConnection> c) {
            connection.Bind(std::move(c));
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_TRUE(connection.is_connected());

  EXPECT_TRUE(contents()->IsConnectedToHidDevice());

  EXPECT_CALL(hid_delegate(),
              DecrementConnectionCount(browser_context(),
                                       url::Origin::Create(GURL(kTestUrl))));
  NavigateAndCommit(GURL(kCrossOriginTestUrl));

  base::RunLoop disconnect_loop;
  connection.set_disconnect_handler(disconnect_loop.QuitClosure());

  disconnect_loop.Run();
  EXPECT_FALSE(contents()->IsConnectedToHidDevice());
  EXPECT_FALSE(connection.is_connected());
}

TEST_P(HidServiceTest, RegisterClient) {
  GetService(GetParam());

  // 1. Connect a device and wait for DeviceAdded.
  auto device_info = CreateDeviceWithOneReport();
  base::RunLoop device_added_loop;
  EXPECT_CALL(hid_manager_client(), DeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop.QuitClosure()));
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  ConnectDevice(*device_info);
  device_added_loop.Run();

  // 2. Disconnect the device and wait for DeviceRemoved.
  base::RunLoop device_removed_loop;
  EXPECT_CALL(hid_manager_client(), DeviceRemoved(_))
      .WillOnce(RunClosure(device_removed_loop.QuitClosure()));
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  DisconnectDevice(*device_info);
  device_removed_loop.Run();
}

TEST_P(HidServiceTest, RevokeDevicePermission) {
  auto service_creation_type = GetParam();
  const auto& service = GetService(service_creation_type);

  // For now the device has permission.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));

  // Create a new device.
  auto device_info = device::mojom::HidDeviceInfo::New();
  device_info->guid = kTestGuid;
  ConnectDevice(*device_info);

  // Connect the device.
  mojo::PendingRemote<device::mojom::HidConnectionClient> hid_connection_client;
  connection_client()->Bind(
      hid_connection_client.InitWithNewPipeAndPassReceiver());

  CheckHidServiceConnectedState(service_creation_type, false);

  base::RunLoop run_loop;
  mojo::Remote<device::mojom::HidConnection> connection;
  EXPECT_CALL(hid_delegate(), GetDeviceInfo)
      .WillOnce(Return(device_info.get()));
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  EXPECT_CALL(hid_delegate(),
              IncrementConnectionCount(GetBrowserContext(service_creation_type),
                                       url::Origin::Create(GURL(kTestUrl))));
  service->Connect(
      kTestGuid, std::move(hid_connection_client),
      base::BindLambdaForTesting(
          [&run_loop,
           &connection](mojo::PendingRemote<device::mojom::HidConnection> c) {
            connection.Bind(std::move(c));
            run_loop.Quit();
          }));
  run_loop.Run();
  testing::Mock::VerifyAndClearExpectations(&hid_delegate());

  CheckHidServiceConnectedState(service_creation_type, true);
  EXPECT_TRUE(connection.is_connected());

  base::RunLoop disconnect_loop;
  connection.set_disconnect_handler(disconnect_loop.QuitClosure());

  // Simulate user revoking permission.
  url::Origin origin = url::Origin::Create(GURL(kTestUrl));
  EXPECT_CALL(hid_delegate(),
              DecrementConnectionCount(GetBrowserContext(service_creation_type),
                                       url::Origin::Create(GURL(kTestUrl))));
  EXPECT_CALL(hid_delegate(), GetDeviceInfo)
      .WillOnce(Return(device_info.get()));
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(false));
  hid_delegate().OnPermissionRevoked(origin);
  testing::Mock::VerifyAndClearExpectations(&hid_delegate());

  disconnect_loop.Run();
  CheckHidServiceConnectedState(service_creation_type, false);
  EXPECT_FALSE(connection.is_connected());
}

TEST_P(HidServiceTest, RevokeDevicePermissionWithoutConnection) {
  auto service_creation_type = GetParam();
  GetService(service_creation_type);

  // Simulate user revoking permission.
  url::Origin origin = url::Origin::Create(GURL(kTestUrl));
  hid_delegate().OnPermissionRevoked(origin);

  base::RunLoop().RunUntilIdle();
  CheckHidServiceConnectedState(service_creation_type, false);
}

TEST_P(HidServiceTest, DeviceRemovedDisconnect) {
  auto service_creation_type = GetParam();
  const auto& service = GetService(service_creation_type);

  // For now the device has permission.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));

  // Create a new device.
  auto device_info = device::mojom::HidDeviceInfo::New();
  device_info->guid = kTestGuid;
  ConnectDevice(*device_info);

  // Connect the device.
  mojo::PendingRemote<device::mojom::HidConnectionClient> hid_connection_client;
  connection_client()->Bind(
      hid_connection_client.InitWithNewPipeAndPassReceiver());

  CheckHidServiceConnectedState(service_creation_type, false);

  base::RunLoop run_loop;
  mojo::Remote<device::mojom::HidConnection> connection;
  EXPECT_CALL(hid_delegate(), GetDeviceInfo)
      .WillOnce(Return(device_info.get()));
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  EXPECT_CALL(hid_delegate(),
              IncrementConnectionCount(GetBrowserContext(service_creation_type),
                                       url::Origin::Create(GURL(kTestUrl))));
  service->Connect(
      kTestGuid, std::move(hid_connection_client),
      base::BindLambdaForTesting(
          [&run_loop,
           &connection](mojo::PendingRemote<device::mojom::HidConnection> c) {
            connection.Bind(std::move(c));
            run_loop.Quit();
          }));
  run_loop.Run();

  CheckHidServiceConnectedState(service_creation_type, true);
  EXPECT_TRUE(connection.is_connected());

  base::RunLoop disconnect_loop;
  connection.set_disconnect_handler(disconnect_loop.QuitClosure());

  // Disconnect the device.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  EXPECT_CALL(hid_delegate(),
              DecrementConnectionCount(GetBrowserContext(service_creation_type),
                                       url::Origin::Create(GURL(kTestUrl))));
  DisconnectDevice(*device_info);

  disconnect_loop.Run();
  CheckHidServiceConnectedState(service_creation_type, false);
  EXPECT_FALSE(connection.is_connected());
}

TEST_P(HidServiceTest, DeviceChangedDoesNotDisconnect) {
  auto service_creation_type = GetParam();
  const auto& service = GetService(service_creation_type);

  // Create a new device.
  base::RunLoop device_added_loop;
  EXPECT_CALL(hid_manager_client(), DeviceAdded)
      .WillOnce(RunClosure(device_added_loop.QuitClosure()));
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  auto device_info = CreateDeviceWithOneReport();
  ConnectDevice(*device_info);
  device_added_loop.Run();

  // Connect the device.
  mojo::PendingRemote<device::mojom::HidConnectionClient> hid_connection_client;
  connection_client()->Bind(
      hid_connection_client.InitWithNewPipeAndPassReceiver());

  CheckHidServiceConnectedState(service_creation_type, false);

  base::RunLoop run_loop;
  mojo::Remote<device::mojom::HidConnection> connection;
  EXPECT_CALL(hid_delegate(), GetDeviceInfo)
      .WillOnce(Return(device_info.get()));
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  EXPECT_CALL(hid_delegate(),
              IncrementConnectionCount(GetBrowserContext(service_creation_type),
                                       url::Origin::Create(GURL(kTestUrl))));
  service->Connect(
      kTestGuid, std::move(hid_connection_client),
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<device::mojom::HidConnection> c) {
            connection.Bind(std::move(c));
            run_loop.Quit();
          }));
  run_loop.Run();

  CheckHidServiceConnectedState(service_creation_type, true);
  EXPECT_TRUE(connection.is_connected());

  // Update the device info. Permissions are not affected.
  auto updated_device_info = CreateDeviceWithTwoReports();
  EXPECT_CALL(hid_delegate(), GetDeviceInfo)
      .WillOnce(Return(updated_device_info.get()));
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  UpdateDevice(*updated_device_info);

  // Make sure the device is still connected.
  CheckHidServiceConnectedState(service_creation_type, true);
  EXPECT_TRUE(connection.is_connected());

  base::RunLoop disconnect_loop;
  connection.set_disconnect_handler(disconnect_loop.QuitClosure());

  // Simulate user revoking permission.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(false));
  url::Origin origin = url::Origin::Create(GURL(kTestUrl));
  EXPECT_CALL(hid_delegate(),
              DecrementConnectionCount(GetBrowserContext(service_creation_type),
                                       url::Origin::Create(GURL(kTestUrl))));
  hid_delegate().OnPermissionRevoked(origin);

  disconnect_loop.Run();
  CheckHidServiceConnectedState(service_creation_type, false);
  EXPECT_FALSE(connection.is_connected());
}

TEST_P(HidServiceTest, UnblockedDeviceChangedToBlockedDisconnects) {
  auto service_creation_type = GetParam();
  const auto& service = GetService(service_creation_type);

  // Create a new device. For now, the device has permission.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  base::RunLoop device_added_loop;
  EXPECT_CALL(hid_manager_client(), DeviceAdded)
      .WillOnce(RunClosure(device_added_loop.QuitClosure()));
  auto device_info = CreateDeviceWithOneReport();
  ConnectDevice(*device_info);
  device_added_loop.Run();

  // Connect the device.
  mojo::PendingRemote<device::mojom::HidConnectionClient> hid_connection_client;
  connection_client()->Bind(
      hid_connection_client.InitWithNewPipeAndPassReceiver());

  CheckHidServiceConnectedState(service_creation_type, false);

  base::RunLoop connect_loop;
  mojo::Remote<device::mojom::HidConnection> connection;
  EXPECT_CALL(hid_delegate(), GetDeviceInfo)
      .WillOnce(Return(device_info.get()));
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  EXPECT_CALL(hid_delegate(),
              IncrementConnectionCount(GetBrowserContext(service_creation_type),
                                       url::Origin::Create(GURL(kTestUrl))));
  service->Connect(
      kTestGuid, std::move(hid_connection_client),
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<device::mojom::HidConnection> c) {
            connection.Bind(std::move(c));
            connect_loop.Quit();
          }));
  connect_loop.Run();

  CheckHidServiceConnectedState(service_creation_type, true);
  EXPECT_TRUE(connection.is_connected());

  // Update the device info. With the update, the device loses permission and
  // the connection is closed.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(false));
  EXPECT_CALL(hid_manager_client(), DeviceRemoved).Times(0);
  EXPECT_CALL(hid_manager_client(), DeviceChanged).Times(0);
  auto updated_device_info = device::mojom::HidDeviceInfo::New();
  updated_device_info->guid = kTestGuid;
  base::RunLoop disconnect_loop;
  connection.set_disconnect_handler(disconnect_loop.QuitClosure());
  EXPECT_CALL(hid_delegate(),
              DecrementConnectionCount(GetBrowserContext(service_creation_type),
                                       url::Origin::Create(GURL(kTestUrl))));
  UpdateDevice(*updated_device_info);
  disconnect_loop.Run();

  CheckHidServiceConnectedState(service_creation_type, false);
  EXPECT_FALSE(connection.is_connected());
}

TEST_P(HidServiceTest, BlockedDeviceChangedToUnblockedDispatchesDeviceChanged) {
  GetService(GetParam());

  // Create a new device. The device is blocked because it has no reports.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  EXPECT_CALL(hid_manager_client(), DeviceAdded).Times(0);
  auto device_info = CreateDeviceWithNoReports();
  ConnectDevice(*device_info);

  // Update the device. After the update, the device has an input report and is
  // no longer blocked. The DeviceChanged event should be dispatched to the
  // client.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  base::RunLoop device_changed_loop;
  EXPECT_CALL(hid_manager_client(), DeviceChanged)
      .WillOnce(RunClosure(device_changed_loop.QuitClosure()));
  auto updated_device_info = CreateDeviceWithOneReport();
  UpdateDevice(*updated_device_info);
  device_changed_loop.Run();

  // Disconnect the device. DeviceRemoved should be dispatched to the client.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  base::RunLoop device_removed_loop;
  EXPECT_CALL(hid_manager_client(), DeviceRemoved)
      .WillOnce(RunClosure(device_removed_loop.QuitClosure()));
  DisconnectDevice(*updated_device_info);
  device_removed_loop.Run();
}

TEST_P(HidServiceTest, Forget) {
  auto service_creation_type = GetParam();
  const auto& service = GetService(service_creation_type);

  // For now the device has permission.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillRepeatedly(Return(true));

  // Create a new device.
  auto device_info = device::mojom::HidDeviceInfo::New();
  device_info->guid = kTestGuid;
  ConnectDevice(*device_info);
  EXPECT_CALL(hid_delegate(), GetDeviceInfo)
      .WillRepeatedly(Return(device_info.get()));

  // Connect the device.
  mojo::PendingRemote<device::mojom::HidConnectionClient> hid_connection_client;
  connection_client()->Bind(
      hid_connection_client.InitWithNewPipeAndPassReceiver());

  CheckHidServiceConnectedState(service_creation_type, false);

  TestFuture<mojo::PendingRemote<device::mojom::HidConnection>>
      future_connection;
  EXPECT_CALL(hid_delegate(),
              IncrementConnectionCount(GetBrowserContext(service_creation_type),
                                       url::Origin::Create(GURL(kTestUrl))));
  service->Connect(kTestGuid, std::move(hid_connection_client),
                   future_connection.GetCallback());
  mojo::Remote<device::mojom::HidConnection> connection(
      future_connection.Take());

  CheckHidServiceConnectedState(service_creation_type, true);
  EXPECT_TRUE(connection.is_connected());

  base::RunLoop disconnect_loop;
  connection.set_disconnect_handler(disconnect_loop.QuitClosure());

  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(false));
  EXPECT_CALL(hid_delegate(), RevokeDevicePermission)
      .WillOnce([this](BrowserContext* browser_context,
                       RenderFrameHost* render_frame_host,
                       const url::Origin& origin,
                       const device::mojom::HidDeviceInfo& device) {
        hid_delegate().OnPermissionRevoked(origin);
      });
  base::MockCallback<blink::mojom::HidService::ForgetCallback> forget_callback;
  EXPECT_CALL(forget_callback, Run);
  EXPECT_CALL(hid_delegate(),
              DecrementConnectionCount(GetBrowserContext(service_creation_type),
                                       url::Origin::Create(GURL(kTestUrl))));
  service->Forget(std::move(device_info), forget_callback.Get());

  disconnect_loop.Run();
  CheckHidServiceConnectedState(service_creation_type, false);
  EXPECT_FALSE(connection.is_connected());
}

TEST_P(HidServiceTest, OpenDevicesThenRemoveDevices) {
  auto service_creation_type = GetParam();
  const auto& service = GetService(service_creation_type);

  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillRepeatedly(Return(true));

  size_t num_devices = 5;
  std::vector<device::mojom::HidDeviceInfoPtr> devices;
  for (size_t device_idx = 0; device_idx < num_devices; device_idx++) {
    auto device_info = device::mojom::HidDeviceInfo::New();
    device_info->guid = base::NumberToString(device_idx);
    ConnectDevice(*device_info);
    devices.push_back(std::move(device_info));
  }
  CheckHidServiceConnectedState(service_creation_type, false);

  std::vector<mojo::Remote<device::mojom::HidConnection>> connections;
  std::vector<FakeHidConnectionClient> connection_clients(num_devices);
  EXPECT_CALL(hid_delegate(), IncrementConnectionCount).Times(num_devices);
  for (size_t device_idx = 0; device_idx < num_devices; device_idx++) {
    EXPECT_CALL(hid_delegate(), GetDeviceInfo)
        .WillOnce(Return(devices[device_idx].get()));
    connections.push_back(OpenDevice(service, devices[device_idx],
                                     connection_clients[device_idx]));
  }
  CheckHidServiceConnectedState(service_creation_type, true);

  base::RunLoop run_loop;
  auto barrier = base::BarrierClosure(num_devices, run_loop.QuitClosure());
  EXPECT_CALL(hid_delegate(), DecrementConnectionCount)
      .Times(num_devices)
      .WillRepeatedly(RunClosure(barrier));
  for (const auto& device : devices) {
    DisconnectDevice(*device);
  }
  run_loop.Run();
  CheckHidServiceConnectedState(service_creation_type, false);
}

TEST_P(HidServiceTest, OpenDevicesThenRevokePermission) {
  auto service_creation_type = GetParam();
  const auto& service = GetService(service_creation_type);

  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillRepeatedly(Return(true));

  size_t num_devices = 5;
  std::vector<device::mojom::HidDeviceInfoPtr> devices;
  for (size_t device_idx = 0; device_idx < num_devices; device_idx++) {
    auto device_info = device::mojom::HidDeviceInfo::New();
    device_info->guid = base::NumberToString(device_idx);
    ConnectDevice(*device_info);
    devices.push_back(std::move(device_info));
  }
  CheckHidServiceConnectedState(service_creation_type, false);

  std::vector<mojo::Remote<device::mojom::HidConnection>> connections;
  std::vector<FakeHidConnectionClient> connection_clients(num_devices);
  EXPECT_CALL(hid_delegate(), IncrementConnectionCount).Times(num_devices);
  for (size_t device_idx = 0; device_idx < num_devices; device_idx++) {
    EXPECT_CALL(hid_delegate(), GetDeviceInfo)
        .WillOnce(Return(devices[device_idx].get()));
    connections.push_back(OpenDevice(service, devices[device_idx],
                                     connection_clients[device_idx]));
  }
  CheckHidServiceConnectedState(service_creation_type, true);

  // Simulate user revoking permission.
  EXPECT_CALL(hid_delegate(), HasDevicePermission)
      .WillRepeatedly(Return(false));
  for (const auto& device : devices) {
    EXPECT_CALL(hid_delegate(), GetDeviceInfo(_, device->guid))
        .WillOnce(Return(device.get()));
  }

  base::RunLoop disconnect_loop;
  auto disconnect_closure =
      base::BarrierClosure(num_devices, disconnect_loop.QuitClosure());
  for (auto& connection : connections) {
    connection.set_disconnect_handler(disconnect_closure);
  }

  base::RunLoop run_loop;
  auto barrier = base::BarrierClosure(num_devices, run_loop.QuitClosure());
  url::Origin origin = url::Origin::Create(GURL(kTestUrl));
  EXPECT_CALL(hid_delegate(),
              DecrementConnectionCount(GetBrowserContext(service_creation_type),
                                       origin))
      .Times(num_devices)
      .WillRepeatedly(RunClosure(barrier));
  hid_delegate().OnPermissionRevoked(origin);

  run_loop.Run();
  disconnect_loop.Run();
  CheckHidServiceConnectedState(service_creation_type, false);
  for (auto& connection : connections) {
    EXPECT_FALSE(connection.is_connected());
  }
}

TEST_P(HidServiceTest, OpenDevicesThenHidServiceReset) {
  auto service_creation_type = GetParam();
  auto& service = GetService(service_creation_type);

  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillRepeatedly(Return(true));

  size_t num_devices = 5;
  std::vector<device::mojom::HidDeviceInfoPtr> devices;
  for (size_t device_idx = 0; device_idx < num_devices; device_idx++) {
    auto device_info = device::mojom::HidDeviceInfo::New();
    device_info->guid = base::NumberToString(device_idx);
    ConnectDevice(*device_info);
    devices.push_back(std::move(device_info));
  }
  CheckHidServiceConnectedState(service_creation_type, false);

  std::vector<mojo::Remote<device::mojom::HidConnection>> connections;
  std::vector<FakeHidConnectionClient> connection_clients(num_devices);
  EXPECT_CALL(hid_delegate(), IncrementConnectionCount).Times(num_devices);
  for (size_t device_idx = 0; device_idx < num_devices; device_idx++) {
    EXPECT_CALL(hid_delegate(), GetDeviceInfo)
        .WillOnce(Return(devices[device_idx].get()));
    connections.push_back(OpenDevice(service, devices[device_idx],
                                     connection_clients[device_idx]));
  }
  CheckHidServiceConnectedState(service_creation_type, true);

  base::RunLoop run_loop;
  auto barrier = base::BarrierClosure(num_devices, run_loop.QuitClosure());
  EXPECT_CALL(hid_delegate(), DecrementConnectionCount)
      .Times(num_devices)
      .WillRepeatedly(RunClosure(barrier));
  service.reset();
  run_loop.Run();
  CheckHidServiceConnectedState(service_creation_type, false);
}

TEST_P(HidServiceFidoTest, FidoDeviceAllowedWithPrivilegedOrigin) {
  auto service_creation_type = std::get<0>(GetParam());
  const auto& service = GetService(service_creation_type);
  const bool is_fido_allowed = std::get<1>(GetParam());

  // Wait for GetDevices to return to ensure the client has been set. HidService
  // checks if the origin is allowed to access FIDO reports before returning the
  // device information to the client.
  url::Origin origin = url::Origin::Create(GURL(kTestUrl));
  EXPECT_CALL(hid_delegate(), IsFidoAllowedForOrigin(_, origin))
      .WillOnce(Return(is_fido_allowed));
  base::RunLoop get_devices_loop;
  service->GetDevices(base::BindLambdaForTesting(
      [&](std::vector<device::mojom::HidDeviceInfoPtr> d) {
        EXPECT_TRUE(d.empty());
        get_devices_loop.Quit();
      }));
  get_devices_loop.Run();

  // Create a FIDO device with two reports. Both reports are protected, which
  // would normally cause the device to be blocked.
  auto device_info = CreateFidoDevice();
  ASSERT_EQ(device_info->collections.size(), 1u);
  ASSERT_EQ(device_info->collections[0]->input_reports.size(), 1u);
  EXPECT_EQ(device_info->collections[0]->input_reports[0]->report_id, 0u);
  ASSERT_EQ(device_info->collections[0]->output_reports.size(), 1u);
  EXPECT_EQ(device_info->collections[0]->output_reports[0]->report_id, 0u);
  EXPECT_TRUE(device_info->collections[0]->feature_reports.empty());
  ASSERT_TRUE(device_info->protected_input_report_ids);
  EXPECT_THAT(*device_info->protected_input_report_ids, ElementsAre(0));
  ASSERT_TRUE(device_info->protected_output_report_ids);
  EXPECT_THAT(*device_info->protected_output_report_ids, ElementsAre(0));
  ASSERT_TRUE(device_info->protected_output_report_ids);
  EXPECT_TRUE(device_info->protected_feature_report_ids->empty());

  // Add the device to the HidManager. HidService checks if the origin is
  // allowed to access FIDO reports before dispatching DeviceAdded to its
  // clients. If the origin is allowed to access FIDO reports, the
  // information about those reports should be included. If the origin is not
  // allowed to access FIDO reports, the device is blocked and DeviceAdded is
  // not called.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  EXPECT_CALL(hid_delegate(), IsFidoAllowedForOrigin(_, origin))
      .WillOnce(Return(is_fido_allowed));
  base::RunLoop device_added_loop;
  if (is_fido_allowed) {
    EXPECT_CALL(hid_manager_client(), DeviceAdded).WillOnce([&](auto d) {
      EXPECT_EQ(d->collections.size(), 1u);
      if (!d->collections.empty()) {
        EXPECT_EQ(d->collections[0]->input_reports.size(), 1u);
        EXPECT_EQ(d->collections[0]->output_reports.size(), 1u);
        EXPECT_EQ(d->collections[0]->feature_reports.size(), 0u);
      }
      device_added_loop.Quit();
    });
  }
  ConnectDevice(*device_info);
  if (is_fido_allowed)
    device_added_loop.Run();

  // Update the device. HidService checks if the origin is allowed to access
  // FIDO reports before dispatching DeviceChanged to its clients.
  //
  // The updated device includes a second top-level collection containing a
  // feature report. The second top-level collection does not have a protected
  // usage and should be included whether or not the origin is allowed to access
  // FIDO reports.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  EXPECT_CALL(hid_delegate(), IsFidoAllowedForOrigin(_, origin))
      .WillOnce(Return(is_fido_allowed));
  base::RunLoop device_changed_loop;
  EXPECT_CALL(hid_manager_client(), DeviceChanged).WillOnce([&](auto d) {
    if (is_fido_allowed) {
      EXPECT_EQ(d->collections.size(), 2u);
      if (d->collections.size() >= 2) {
        EXPECT_EQ(d->collections[0]->input_reports.size(), 1u);
        EXPECT_EQ(d->collections[0]->output_reports.size(), 1u);
        EXPECT_EQ(d->collections[0]->feature_reports.size(), 0u);
        EXPECT_EQ(d->collections[1]->input_reports.size(), 0u);
        EXPECT_EQ(d->collections[1]->output_reports.size(), 0u);
        EXPECT_EQ(d->collections[1]->feature_reports.size(), 1u);
      }
    } else {
      EXPECT_EQ(d->collections.size(), 1u);
      if (d->collections.size() >= 1) {
        EXPECT_EQ(d->collections[0]->input_reports.size(), 0u);
        EXPECT_EQ(d->collections[0]->output_reports.size(), 0u);
        EXPECT_EQ(d->collections[0]->feature_reports.size(), 1u);
      }
    }
    device_changed_loop.Quit();
  });
  auto collection = device::mojom::HidCollectionInfo::New();
  collection->usage = device::mojom::HidUsageAndPage::New(
      device::mojom::kGenericDesktopJoystick,
      device::mojom::kPageGenericDesktop);
  collection->collection_type = device::mojom::kHIDCollectionTypeApplication;
  collection->feature_reports.push_back(
      device::mojom::HidReportDescription::New());
  auto updated_device_info = device_info.Clone();
  updated_device_info->collections.push_back(std::move(collection));
  UpdateDevice(*updated_device_info);
  device_changed_loop.Run();

  // Open a connection. HidService checks if the origin is allowed to access
  // FIDO reports before creating a HidConnection.
  EXPECT_CALL(hid_delegate(), IsFidoAllowedForOrigin(_, origin))
      .WillOnce(Return(is_fido_allowed));
  mojo::PendingRemote<device::mojom::HidConnectionClient> hid_connection_client;
  connection_client()->Bind(
      hid_connection_client.InitWithNewPipeAndPassReceiver());
  base::RunLoop connect_loop;
  mojo::Remote<device::mojom::HidConnection> connection;
  EXPECT_CALL(hid_delegate(), GetDeviceInfo)
      .WillOnce(Return(device_info.get()));
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  EXPECT_CALL(hid_delegate(),
              IncrementConnectionCount(GetBrowserContext(service_creation_type),
                                       url::Origin::Create(GURL(kTestUrl))));
  service->Connect(
      device_info->guid, std::move(hid_connection_client),
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<device::mojom::HidConnection> c) {
            connection.Bind(std::move(c));
            connect_loop.Quit();
          }));
  connect_loop.Run();
  EXPECT_TRUE(connection.is_connected());

  // Try reading from the connection. The read should succeed if the connection
  // is allowed to receive FIDO reports.
  base::RunLoop read_loop;
  connection->Read(base::BindLambdaForTesting(
      [&](bool success, uint8_t report_id,
          const std::optional<std::vector<uint8_t>>& buffer) {
        EXPECT_EQ(success, is_fido_allowed);
        read_loop.Quit();
      }));
  read_loop.Run();

  // Try writing to the connection. The write should succeed if the connection
  // is allowed to send FIDO reports.
  //
  // Writing to FakeHidConnection will only succeed if the report data is
  // exactly "o-report".
  base::RunLoop write_loop;
  std::vector<uint8_t> buffer = {'o', '-', 'r', 'e', 'p', 'o', 'r', 't'};
  connection->Write(/*report_id=*/0, buffer,
                    base::BindLambdaForTesting([&](bool success) {
                      EXPECT_EQ(success, is_fido_allowed);
                      write_loop.Quit();
                    }));
  write_loop.Run();

  // Disconnect the device. HidService checks if the origin is allowed to access
  // FIDO reports before dispatching DeviceRemoved to its clients. The
  // information about FIDO reports should only be included if the origin is
  // allowed to access FIDO reports.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  EXPECT_CALL(hid_delegate(), IsFidoAllowedForOrigin(_, origin))
      .WillOnce(Return(is_fido_allowed));
  base::RunLoop device_removed_loop;
  EXPECT_CALL(hid_manager_client(), DeviceRemoved).WillOnce([&](auto d) {
    if (is_fido_allowed) {
      EXPECT_EQ(d->collections.size(), 2u);
      if (d->collections.size() >= 2) {
        EXPECT_EQ(d->collections[0]->input_reports.size(), 1u);
        EXPECT_EQ(d->collections[0]->output_reports.size(), 1u);
        EXPECT_EQ(d->collections[0]->feature_reports.size(), 0u);
        EXPECT_EQ(d->collections[1]->input_reports.size(), 0u);
        EXPECT_EQ(d->collections[1]->output_reports.size(), 0u);
        EXPECT_EQ(d->collections[1]->feature_reports.size(), 1u);
      }
    } else {
      EXPECT_EQ(d->collections.size(), 1u);
      if (d->collections.size() >= 1) {
        EXPECT_EQ(d->collections[0]->input_reports.size(), 0u);
        EXPECT_EQ(d->collections[0]->output_reports.size(), 0u);
        EXPECT_EQ(d->collections[0]->feature_reports.size(), 1u);
      }
    }
    device_removed_loop.Quit();
  });
  EXPECT_CALL(hid_delegate(),
              DecrementConnectionCount(GetBrowserContext(service_creation_type),
                                       url::Origin::Create(GURL(kTestUrl))));
  DisconnectDevice(*updated_device_info);
  device_removed_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(
    HidServiceTests,
    HidServiceTest,
    testing::Values(kCreateUsingRenderFrameHost,
                    kCreateUsingServiceWorkerContextCore),
    [](const ::testing::TestParamInfo<HidServiceCreationType>& info) {
      return HidServiceCreationTypeToString(info.param);
    });

const bool kIsFidoAllowed[]{true, false};
INSTANTIATE_TEST_SUITE_P(
    HidServiceFidoTests,
    HidServiceFidoTest,
    testing::Combine(testing::Values(kCreateUsingRenderFrameHost,
                                     kCreateUsingServiceWorkerContextCore),
                     testing::ValuesIn(kIsFidoAllowed)),
    [](const ::testing::TestParamInfo<std::tuple<HidServiceCreationType, bool>>&
           info) {
      return base::StringPrintf(
          "%s_%s",
          HidServiceCreationTypeToString(std::get<0>(info.param)).c_str(),
          std::get<1>(info.param) ? "FidoAllowed" : "FidoNotAllowed");
    });

TEST_F(HidServiceServiceWorkerBrowserContextDestroyedTest, GetDevices) {
  auto device_info = CreateDeviceWithOneReport();
  ConnectDevice(*device_info);
  DestroyBrowserContext();

  TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>> devices_future;
  service_->GetDevices(devices_future.GetCallback());
  EXPECT_EQ(0u, devices_future.Get().size());
}

TEST_F(HidServiceServiceWorkerBrowserContextDestroyedTest, Connect) {
  auto device_info = CreateDeviceWithOneReport();
  ConnectDevice(*device_info);
  mojo::PendingRemote<device::mojom::HidConnectionClient> hid_connection_client;
  connection_client()->Bind(
      hid_connection_client.InitWithNewPipeAndPassReceiver());
  DestroyBrowserContext();

  TestFuture<mojo::PendingRemote<device::mojom::HidConnection>>
      connection_future;
  service_->Connect(kTestGuid, std::move(hid_connection_client),
                    connection_future.GetCallback());
  EXPECT_FALSE(connection_future.Get().is_valid());
}

TEST_F(HidServiceServiceWorkerBrowserContextDestroyedTest, Forget) {
  auto device_info = CreateDeviceWithOneReport();
  ConnectDevice(*device_info);

  DestroyBrowserContext();
  EXPECT_CALL(hid_delegate(), RevokeDevicePermission).Times(0);
  base::RunLoop run_loop;
  service_->Forget(std::move(device_info), run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(HidServiceServiceWorkerBrowserContextDestroyedTest, RejectOpaqueOrigin) {
  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(std::string());
  response_headers->SetHeader("Content-Security-Policy",
                              "sandbox allow-scripts");
  auto* web_contents = static_cast<TestWebContents*>(
      web_contents_factory_.CreateWebContents(&browser_context_));
  auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
      GURL("http://whatever.com"), web_contents->GetPrimaryMainFrame());
  navigation_simulator->SetResponseHeaders(response_headers);
  navigation_simulator->Start();
  navigation_simulator->Commit();

  mojo::Remote<blink::mojom::HidService> service;
  web_contents->GetPrimaryMainFrame()->GetHidService(
      service.BindNewPipeAndPassReceiver());

  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "WebHID is not allowed from an opaque origin.");
}

TEST_P(HidServiceTest, ConnectionFailedWithoutPermission) {
  auto service_creation_type = GetParam();
  const auto& service = GetService(service_creation_type);

  // Create a new device.
  auto device_info = device::mojom::HidDeviceInfo::New();
  device_info->guid = kTestGuid;
  ConnectDevice(*device_info);

  // Connect the device.
  mojo::PendingRemote<device::mojom::HidConnectionClient> hid_connection_client;
  connection_client()->Bind(
      hid_connection_client.InitWithNewPipeAndPassReceiver());

  CheckHidServiceConnectedState(service_creation_type, false);

  TestFuture<mojo::PendingRemote<device::mojom::HidConnection>>
      pending_remote_future;
  EXPECT_CALL(hid_delegate(), GetDeviceInfo)
      .WillOnce(Return(device_info.get()));
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(false));
  service->Connect(kTestGuid, std::move(hid_connection_client),
                   pending_remote_future.GetCallback());
  EXPECT_FALSE(pending_remote_future.Take());
  CheckHidServiceConnectedState(service_creation_type, false);
}

}  // namespace content
