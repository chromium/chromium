// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "content/browser/hid/hid_test_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/hid_delegate.h"
#include "content/public/common/content_client.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/hid/test_report_descriptors.h"
#include "services/device/hid/test_util.h"
#include "services/device/public/cpp/hid/fake_hid_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"

using ::base::test::RunClosure;
using ::testing::_;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Return;

namespace content {

namespace {

const char kTestUrl[] = "https://www.google.com";
const char kTestGuid[] = "test-guid";
const char kCrossOriginTestUrl[] = "https://www.chromium.org";

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

// Main test fixture.
class HidServiceTest : public RenderViewHostImplTestHarness {
 public:
  HidServiceTest() {
    ON_CALL(hid_delegate(), GetHidManager).WillByDefault(Return(&hid_manager_));
    ON_CALL(hid_delegate(), IsFidoAllowedForOrigin)
        .WillByDefault(Return(false));
  }
  HidServiceTest(HidServiceTest&) = delete;
  HidServiceTest& operator=(HidServiceTest&) = delete;
  ~HidServiceTest() override = default;

  void SetUp() override {
    original_client_ = SetBrowserClientForTesting(&test_client_);
    RenderViewHostTestHarness::SetUp();
  }

  void TearDown() override {
    RenderViewHostTestHarness::TearDown();
    if (original_client_)
      SetBrowserClientForTesting(original_client_);
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

  MockHidDelegate& hid_delegate() { return test_client_.delegate(); }
  FakeHidConnectionClient* connection_client() { return &connection_client_; }

 private:
  HidTestContentBrowserClient test_client_;
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;
  device::FakeHidManager hid_manager_;
  FakeHidConnectionClient connection_client_;
};

}  // namespace

TEST_F(HidServiceTest, GetDevicesWithPermission) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::HidService> service;
  contents()->GetMainFrame()->GetHidService(
      service.BindNewPipeAndPassReceiver());

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

TEST_F(HidServiceTest, GetDevicesWithoutPermission) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::HidService> service;
  contents()->GetMainFrame()->GetHidService(
      service.BindNewPipeAndPassReceiver());

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

TEST_F(HidServiceTest, RequestDevice) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::HidService> service;
  contents()->GetMainFrame()->GetHidService(
      service.BindNewPipeAndPassReceiver());

  auto device_info = CreateDeviceWithOneReport();
  std::vector<device::mojom::HidDeviceInfoPtr> device_infos;
  device_infos.push_back(device_info.Clone());
  ConnectDevice(*device_info);

  EXPECT_CALL(hid_delegate(), CanRequestDevicePermission)
      .WillOnce(Return(true));
  EXPECT_CALL(hid_delegate(), RunChooserInternal)
      .WillOnce(Return(ByMove(std::move(device_infos))));

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
  EXPECT_EQ(1u, chosen_devices.size());
}

TEST_F(HidServiceTest, OpenAndCloseHidConnection) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::HidService> service;
  contents()->GetMainFrame()->GetHidService(
      service.BindNewPipeAndPassReceiver());

  auto device_info = CreateDeviceWithOneReport();
  ConnectDevice(*device_info);

  mojo::PendingRemote<device::mojom::HidConnectionClient> hid_connection_client;
  connection_client()->Bind(
      hid_connection_client.InitWithNewPipeAndPassReceiver());

  EXPECT_FALSE(contents()->IsConnectedToHidDevice());

  base::RunLoop run_loop;
  mojo::Remote<device::mojom::HidConnection> connection;
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

  // Destroying |connection| will also disconnect the watcher.
  connection.reset();

  // Allow the watcher's disconnect handler to run. This will update the
  // WebContents active frame count.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(contents()->IsConnectedToHidDevice());
}

// This test is disabled because it fails on the "linux-bfcache-rel" bot.
// TODO(https://crbug.com/1232841): Re-enable this test.
TEST_F(HidServiceTest, DISABLED_OpenAndNavigateCrossOrigin) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::HidService> service;
  contents()->GetMainFrame()->GetHidService(
      service.BindNewPipeAndPassReceiver());

  auto device_info = CreateDeviceWithOneReport();
  ConnectDevice(*device_info);

  mojo::PendingRemote<device::mojom::HidConnectionClient> hid_connection_client;
  connection_client()->Bind(
      hid_connection_client.InitWithNewPipeAndPassReceiver());

  EXPECT_FALSE(contents()->IsConnectedToHidDevice());

  base::RunLoop run_loop;
  mojo::Remote<device::mojom::HidConnection> connection;
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

  NavigateAndCommit(GURL(kCrossOriginTestUrl));

  base::RunLoop disconnect_loop;
  connection.set_disconnect_handler(disconnect_loop.QuitClosure());

  disconnect_loop.Run();
  EXPECT_FALSE(contents()->IsConnectedToHidDevice());
  EXPECT_FALSE(connection.is_connected());
}

TEST_F(HidServiceTest, RegisterClient) {
  MockHidManagerClient mock_hid_manager_client;

  base::RunLoop device_added_loop;
  EXPECT_CALL(mock_hid_manager_client, DeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop.QuitClosure()));

  base::RunLoop device_removed_loop;
  EXPECT_CALL(mock_hid_manager_client, DeviceRemoved(_))
      .WillOnce(RunClosure(device_removed_loop.QuitClosure()));

  EXPECT_CALL(hid_delegate(), HasDevicePermission)
      .WillOnce(Return(true))
      .WillOnce(Return(true));

  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::HidService> service;
  contents()->GetMainFrame()->GetHidService(
      service.BindNewPipeAndPassReceiver());

  mojo::PendingAssociatedRemote<device::mojom::HidManagerClient>
      hid_manager_client;
  mock_hid_manager_client.Bind(
      hid_manager_client.InitWithNewEndpointAndPassReceiver());

  // 1. Register the mock client with the service. Wait for GetDevices to
  // return to ensure the client has been set.
  service->RegisterClient(std::move(hid_manager_client));

  base::RunLoop run_loop;
  std::vector<device::mojom::HidDeviceInfoPtr> devices;
  service->GetDevices(base::BindLambdaForTesting(
      [&run_loop, &devices](std::vector<device::mojom::HidDeviceInfoPtr> d) {
        devices = std::move(d);
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_TRUE(devices.empty());

  // 2. Connect a device and wait for DeviceAdded.
  auto device_info = CreateDeviceWithOneReport();
  ConnectDevice(*device_info);
  device_added_loop.Run();

  // 3. Disconnect the device and wait for DeviceRemoved.
  DisconnectDevice(*device_info);
  device_removed_loop.Run();
}

TEST_F(HidServiceTest, RevokeDevicePermission) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::HidService> service;
  contents()->GetMainFrame()->GetHidService(
      service.BindNewPipeAndPassReceiver());

  // For now the device has permission.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));

  // Create a new device.
  auto device_info = device::mojom::HidDeviceInfo::New();
  device_info->guid = kTestGuid;
  ConnectDevice(*device_info);
  EXPECT_CALL(hid_delegate(), GetDeviceInfo)
      .WillOnce(Return(device_info.get()));

  // Connect the device.
  mojo::PendingRemote<device::mojom::HidConnectionClient> hid_connection_client;
  connection_client()->Bind(
      hid_connection_client.InitWithNewPipeAndPassReceiver());

  EXPECT_FALSE(contents()->IsConnectedToHidDevice());

  base::RunLoop run_loop;
  mojo::Remote<device::mojom::HidConnection> connection;
  service->Connect(
      kTestGuid, std::move(hid_connection_client),
      base::BindLambdaForTesting(
          [&run_loop,
           &connection](mojo::PendingRemote<device::mojom::HidConnection> c) {
            connection.Bind(std::move(c));
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(contents()->IsConnectedToHidDevice());
  EXPECT_TRUE(connection.is_connected());

  base::RunLoop disconnect_loop;
  connection.set_disconnect_handler(disconnect_loop.QuitClosure());

  // Simulate user revoking permission.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(false));
  url::Origin origin = url::Origin::Create(GURL(kTestUrl));
  hid_delegate().OnPermissionRevoked(origin);

  disconnect_loop.Run();
  EXPECT_FALSE(contents()->IsConnectedToHidDevice());
  EXPECT_FALSE(connection.is_connected());
}

TEST_F(HidServiceTest, RevokeDevicePermissionWithoutConnection) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::HidService> service;
  contents()->GetMainFrame()->GetHidService(
      service.BindNewPipeAndPassReceiver());

  // Simulate user revoking permission.
  url::Origin origin = url::Origin::Create(GURL(kTestUrl));
  hid_delegate().OnPermissionRevoked(origin);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(contents()->IsConnectedToHidDevice());
}

TEST_F(HidServiceTest, DeviceRemovedDisconnect) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::HidService> service;
  contents()->GetMainFrame()->GetHidService(
      service.BindNewPipeAndPassReceiver());

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

  EXPECT_FALSE(contents()->IsConnectedToHidDevice());

  base::RunLoop run_loop;
  mojo::Remote<device::mojom::HidConnection> connection;
  service->Connect(
      kTestGuid, std::move(hid_connection_client),
      base::BindLambdaForTesting(
          [&run_loop,
           &connection](mojo::PendingRemote<device::mojom::HidConnection> c) {
            connection.Bind(std::move(c));
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(contents()->IsConnectedToHidDevice());
  EXPECT_TRUE(connection.is_connected());

  base::RunLoop disconnect_loop;
  connection.set_disconnect_handler(disconnect_loop.QuitClosure());

  // Disconnect the device.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  DisconnectDevice(*device_info);

  disconnect_loop.Run();
  EXPECT_FALSE(contents()->IsConnectedToHidDevice());
  EXPECT_FALSE(connection.is_connected());
}

TEST_F(HidServiceTest, DeviceChangedDoesNotDisconnect) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::HidService> service;
  contents()->GetMainFrame()->GetHidService(
      service.BindNewPipeAndPassReceiver());

  // Register the mock client with the service. Wait for GetDevices to return to
  // ensure the client has been set.
  MockHidManagerClient mock_hid_manager_client;
  mojo::PendingAssociatedRemote<device::mojom::HidManagerClient>
      hid_manager_client;
  mock_hid_manager_client.Bind(
      hid_manager_client.InitWithNewEndpointAndPassReceiver());
  service->RegisterClient(std::move(hid_manager_client));

  base::RunLoop get_devices_loop;
  service->GetDevices(base::BindLambdaForTesting(
      [&](std::vector<device::mojom::HidDeviceInfoPtr> d) {
        get_devices_loop.Quit();
      }));
  get_devices_loop.Run();

  // Create a new device.
  base::RunLoop device_added_loop;
  EXPECT_CALL(mock_hid_manager_client, DeviceAdded)
      .WillOnce(RunClosure(device_added_loop.QuitClosure()));
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  auto device_info = CreateDeviceWithOneReport();
  ConnectDevice(*device_info);
  device_added_loop.Run();

  // Connect the device.
  mojo::PendingRemote<device::mojom::HidConnectionClient> hid_connection_client;
  connection_client()->Bind(
      hid_connection_client.InitWithNewPipeAndPassReceiver());

  EXPECT_FALSE(contents()->IsConnectedToHidDevice());

  base::RunLoop run_loop;
  mojo::Remote<device::mojom::HidConnection> connection;
  service->Connect(
      kTestGuid, std::move(hid_connection_client),
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<device::mojom::HidConnection> c) {
            connection.Bind(std::move(c));
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(contents()->IsConnectedToHidDevice());
  EXPECT_TRUE(connection.is_connected());

  // Update the device info. Permissions are not affected.
  auto updated_device_info = CreateDeviceWithTwoReports();
  EXPECT_CALL(hid_delegate(), GetDeviceInfo)
      .WillOnce(Return(updated_device_info.get()));
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  UpdateDevice(*updated_device_info);

  // Make sure the device is still connected.
  EXPECT_TRUE(contents()->IsConnectedToHidDevice());
  EXPECT_TRUE(connection.is_connected());

  base::RunLoop disconnect_loop;
  connection.set_disconnect_handler(disconnect_loop.QuitClosure());

  // Simulate user revoking permission.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(false));
  url::Origin origin = url::Origin::Create(GURL(kTestUrl));
  hid_delegate().OnPermissionRevoked(origin);

  disconnect_loop.Run();
  EXPECT_FALSE(contents()->IsConnectedToHidDevice());
  EXPECT_FALSE(connection.is_connected());
}

TEST_F(HidServiceTest, UnblockedDeviceChangedToBlockedDisconnects) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::HidService> service;
  contents()->GetMainFrame()->GetHidService(
      service.BindNewPipeAndPassReceiver());

  // Register the mock client with the service. Wait for GetDevices to return to
  // ensure the client has been set.
  MockHidManagerClient mock_hid_manager_client;
  mojo::PendingAssociatedRemote<device::mojom::HidManagerClient>
      hid_manager_client;
  mock_hid_manager_client.Bind(
      hid_manager_client.InitWithNewEndpointAndPassReceiver());
  service->RegisterClient(std::move(hid_manager_client));

  base::RunLoop get_devices_loop;
  service->GetDevices(base::BindLambdaForTesting(
      [&](std::vector<device::mojom::HidDeviceInfoPtr> d) {
        get_devices_loop.Quit();
      }));
  get_devices_loop.Run();

  // Create a new device. For now, the device has permission.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  base::RunLoop device_added_loop;
  EXPECT_CALL(mock_hid_manager_client, DeviceAdded)
      .WillOnce(RunClosure(device_added_loop.QuitClosure()));
  auto device_info = CreateDeviceWithOneReport();
  ConnectDevice(*device_info);
  device_added_loop.Run();

  // Connect the device.
  mojo::PendingRemote<device::mojom::HidConnectionClient> hid_connection_client;
  connection_client()->Bind(
      hid_connection_client.InitWithNewPipeAndPassReceiver());

  EXPECT_FALSE(contents()->IsConnectedToHidDevice());

  base::RunLoop connect_loop;
  mojo::Remote<device::mojom::HidConnection> connection;
  service->Connect(
      kTestGuid, std::move(hid_connection_client),
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<device::mojom::HidConnection> c) {
            connection.Bind(std::move(c));
            connect_loop.Quit();
          }));
  connect_loop.Run();

  EXPECT_TRUE(contents()->IsConnectedToHidDevice());
  EXPECT_TRUE(connection.is_connected());

  // Update the device info. With the update, the device loses permission and
  // the connection is closed.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(false));
  EXPECT_CALL(mock_hid_manager_client, DeviceRemoved).Times(0);
  EXPECT_CALL(mock_hid_manager_client, DeviceChanged).Times(0);
  auto updated_device_info = device::mojom::HidDeviceInfo::New();
  updated_device_info->guid = kTestGuid;
  base::RunLoop disconnect_loop;
  connection.set_disconnect_handler(disconnect_loop.QuitClosure());
  UpdateDevice(*updated_device_info);
  disconnect_loop.Run();

  EXPECT_FALSE(contents()->IsConnectedToHidDevice());
  EXPECT_FALSE(connection.is_connected());
}

TEST_F(HidServiceTest, BlockedDeviceChangedToUnblockedDispatchesDeviceChanged) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::HidService> service;
  contents()->GetMainFrame()->GetHidService(
      service.BindNewPipeAndPassReceiver());

  // Register the mock client with the service. Wait for GetDevices to return to
  // ensure the client has been set.
  MockHidManagerClient mock_hid_manager_client;
  mojo::PendingAssociatedRemote<device::mojom::HidManagerClient>
      hid_manager_client;
  mock_hid_manager_client.Bind(
      hid_manager_client.InitWithNewEndpointAndPassReceiver());
  service->RegisterClient(std::move(hid_manager_client));

  base::RunLoop get_devices_loop;
  service->GetDevices(base::BindLambdaForTesting(
      [&](std::vector<device::mojom::HidDeviceInfoPtr> d) {
        get_devices_loop.Quit();
      }));
  get_devices_loop.Run();

  // Create a new device. The device is blocked because it has no reports.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  EXPECT_CALL(mock_hid_manager_client, DeviceAdded).Times(0);
  auto device_info = CreateDeviceWithNoReports();
  ConnectDevice(*device_info);

  // Update the device. After the update, the device has an input report and is
  // no longer blocked. The DeviceChanged event should be dispatched to the
  // client.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  base::RunLoop device_changed_loop;
  EXPECT_CALL(mock_hid_manager_client, DeviceChanged)
      .WillOnce(RunClosure(device_changed_loop.QuitClosure()));
  auto updated_device_info = CreateDeviceWithOneReport();
  UpdateDevice(*updated_device_info);
  device_changed_loop.Run();

  // Disconnect the device. DeviceRemoved should be dispatched to the client.
  EXPECT_CALL(hid_delegate(), HasDevicePermission).WillOnce(Return(true));
  base::RunLoop device_removed_loop;
  EXPECT_CALL(mock_hid_manager_client, DeviceRemoved)
      .WillOnce(RunClosure(device_removed_loop.QuitClosure()));
  DisconnectDevice(*updated_device_info);
  device_removed_loop.Run();
}

class HidServiceFidoTest : public HidServiceTest,
                           public testing::WithParamInterface<bool> {};

TEST_P(HidServiceFidoTest, FidoDeviceAllowedWithPrivilegedOrigin) {
  const bool is_fido_allowed = GetParam();
  GURL test_url = GURL(kTestUrl);
  NavigateAndCommit(test_url);

  mojo::Remote<blink::mojom::HidService> service;
  contents()->GetMainFrame()->GetHidService(
      service.BindNewPipeAndPassReceiver());

  // Register the mock client with the service.
  MockHidManagerClient mock_hid_manager_client;
  mojo::PendingAssociatedRemote<device::mojom::HidManagerClient>
      hid_manager_client;
  mock_hid_manager_client.Bind(
      hid_manager_client.InitWithNewEndpointAndPassReceiver());
  service->RegisterClient(std::move(hid_manager_client));

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
    EXPECT_CALL(mock_hid_manager_client, DeviceAdded).WillOnce([&](auto d) {
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
  EXPECT_CALL(mock_hid_manager_client, DeviceChanged).WillOnce([&](auto d) {
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
          const absl::optional<std::vector<uint8_t>>& buffer) {
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
  EXPECT_CALL(mock_hid_manager_client, DeviceRemoved).WillOnce([&](auto d) {
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
  DisconnectDevice(*updated_device_info);
  device_removed_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(HidServiceFidoTests,
                         HidServiceFidoTest,
                         testing::Values(false, true));

}  // namespace content
