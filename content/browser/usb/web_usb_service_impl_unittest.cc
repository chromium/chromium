// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/usb/web_usb_service_impl.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "content/browser/usb/usb_test_utils.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::base::test::RunOnceCallback;
using ::base::test::RunOnceClosure;
using ::base::test::TestFuture;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

constexpr base::StringPiece kDefaultTestUrl{"https://www.google.com/"};
constexpr base::StringPiece kCrossOriginTestUrl{"https://www.chromium.org"};

class MockWebContentsObserver : public WebContentsObserver {
 public:
  explicit MockWebContentsObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  MockWebContentsObserver(const MockWebContentsObserver&) = delete;
  MockWebContentsObserver& operator=(const MockWebContentsObserver&) = delete;
  ~MockWebContentsObserver() override = default;

  MOCK_METHOD1(OnIsConnectedToUsbDeviceChanged, void(bool));
};

// Test fixture for WebUsbServiceImpl unit tests.
class WebUsbServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  WebUsbServiceImplTest() = default;
  WebUsbServiceImplTest(const WebUsbServiceImplTest&) = delete;
  WebUsbServiceImplTest& operator=(const WebUsbServiceImplTest&) = delete;

  void SetUp() override {
    original_client_ = SetBrowserClientForTesting(&test_client_);
    RenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL(kDefaultTestUrl));
  }

  void TearDown() override {
    RenderViewHostTestHarness::TearDown();
    SetBrowserClientForTesting(original_client_);
  }

 protected:
  void SimulateDeviceServiceCrash() { device_manager()->CloseAllBindings(); }

  void ConnectToService(
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
    contents()->GetPrimaryMainFrame()->CreateWebUsbService(std::move(receiver));

    // Set the fake device manager.
    if (!device_manager()->IsBound()) {
      mojo::PendingRemote<device::mojom::UsbDeviceManager>
          pending_device_manager;
      device_manager()->AddReceiver(
          pending_device_manager.InitWithNewPipeAndPassReceiver());
    }

    // For tests, all devices are permitted by default.
    ON_CALL(delegate(), HasDevicePermission).WillByDefault(Return(true));

    // Forward calls to the fake device manager.
    ON_CALL(delegate(), GetDevices)
        .WillByDefault(
            [&](auto& frame,
                device::mojom::UsbDeviceManager::GetDevicesCallback callback) {
              device_manager()->GetDevices(nullptr, std::move(callback));
            });
    ON_CALL(delegate(), GetDevice)
        .WillByDefault(
            [&](auto& frame, const std::string& guid,
                base::span<const uint8_t> blocked_interface_classes,
                mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
                mojo::PendingRemote<device::mojom::UsbDeviceClient>
                    device_client) {
              device_manager()->GetDevice(
                  guid,
                  std::vector<uint8_t>(blocked_interface_classes.begin(),
                                       blocked_interface_classes.end()),
                  std::move(device_receiver), std::move(device_client));
            });
    ON_CALL(delegate(), GetDeviceInfo)
        .WillByDefault([&](auto& frame, const std::string& guid) {
          return device_manager()->GetDeviceInfo(guid);
        });
  }

  device::FakeUsbDeviceManager* device_manager() {
    if (!device_manager_) {
      device_manager_ = std::make_unique<device::FakeUsbDeviceManager>();
    }
    return device_manager_.get();
  }

  MockUsbDelegate& delegate() { return test_client_.delegate(); }

 private:
  UsbTestContentBrowserClient test_client_;
  raw_ptr<ContentBrowserClient> original_client_;
  std::unique_ptr<device::FakeUsbDeviceManager> device_manager_;
};

void GetDevicesBlocking(blink::mojom::WebUsbService* service,
                        const std::set<std::string>& expected_guids) {
  TestFuture<std::vector<device::mojom::UsbDeviceInfoPtr>> get_devices_future;
  service->GetDevices(get_devices_future.GetCallback());
  ASSERT_TRUE(get_devices_future.Wait());
  EXPECT_EQ(expected_guids.size(), get_devices_future.Get().size());
  std::set<std::string> actual_guids;
  for (const auto& device : get_devices_future.Get())
    actual_guids.insert(device->guid);
  EXPECT_EQ(expected_guids, actual_guids);
}

void OpenDeviceBlocking(device::mojom::UsbDevice* device) {
  TestFuture<device::mojom::UsbOpenDeviceError> open_future;
  device->Open(open_future.GetCallback());
  EXPECT_EQ(open_future.Get(), device::mojom::UsbOpenDeviceError::OK);
}

}  // namespace

TEST_F(WebUsbServiceImplTest, OpenAndCloseDevice) {
  const auto origin = url::Origin::Create(GURL(kDefaultTestUrl));
  MockWebContentsObserver web_contents_observer(contents());

  mojo::Remote<blink::mojom::WebUsbService> service;
  ConnectToService(service.BindNewPipeAndPassReceiver());

  auto device_info = device_manager()->CreateAndAddDevice(
      0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  device::MockUsbMojoDevice mock_device;
  device_manager()->SetMockForDevice(device_info->guid, &mock_device);

  GetDevicesBlocking(service.get(), {device_info->guid});

  mojo::Remote<device::mojom::UsbDevice> device;
  service->GetDevice(device_info->guid, device.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(contents()->IsConnectedToUsbDevice());

  EXPECT_CALL(web_contents_observer, OnIsConnectedToUsbDeviceChanged(true));
  EXPECT_CALL(mock_device, Open)
      .WillOnce(RunOnceCallback<0>(device::mojom::UsbOpenDeviceError::OK));
  OpenDeviceBlocking(device.get());
  EXPECT_TRUE(contents()->IsConnectedToUsbDevice());

  EXPECT_CALL(web_contents_observer, OnIsConnectedToUsbDeviceChanged(false));
  EXPECT_CALL(mock_device, Close).WillOnce(RunOnceClosure<0>());
  {
    base::RunLoop run_loop;
    device->Close(run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_FALSE(contents()->IsConnectedToUsbDevice());
}

TEST_F(WebUsbServiceImplTest, OpenAndDisconnectDevice) {
  const auto origin = url::Origin::Create(GURL(kDefaultTestUrl));
  MockWebContentsObserver web_contents_observer(contents());

  auto fake_device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  auto device_info = device_manager()->AddDevice(fake_device);
  device::MockUsbMojoDevice mock_device;
  device_manager()->SetMockForDevice(device_info->guid, &mock_device);

  mojo::Remote<blink::mojom::WebUsbService> service;
  ConnectToService(service.BindNewPipeAndPassReceiver());

  GetDevicesBlocking(service.get(), {device_info->guid});

  mojo::Remote<device::mojom::UsbDevice> device;
  service->GetDevice(device_info->guid, device.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(contents()->IsConnectedToUsbDevice());

  EXPECT_CALL(web_contents_observer, OnIsConnectedToUsbDeviceChanged(true));
  EXPECT_CALL(mock_device, Open)
      .WillOnce(RunOnceCallback<0>(device::mojom::UsbOpenDeviceError::OK));
  OpenDeviceBlocking(device.get());
  EXPECT_TRUE(contents()->IsConnectedToUsbDevice());

  base::RunLoop loop;
  EXPECT_CALL(web_contents_observer, OnIsConnectedToUsbDeviceChanged(false))
      .WillOnce([&loop](bool is_connected_to_usb_device) { loop.Quit(); });
  device_manager()->RemoveDevice(fake_device);
  loop.Run();
  EXPECT_FALSE(contents()->IsConnectedToUsbDevice());
}

TEST_F(WebUsbServiceImplTest, OpenAndNavigateCrossOrigin) {
  const auto origin = url::Origin::Create(GURL(kDefaultTestUrl));
  MockWebContentsObserver web_contents_observer(contents());

  auto fake_device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  auto device_info = device_manager()->AddDevice(fake_device);

  mojo::Remote<blink::mojom::WebUsbService> service;
  ConnectToService(service.BindNewPipeAndPassReceiver());

  GetDevicesBlocking(service.get(), {device_info->guid});

  mojo::Remote<device::mojom::UsbDevice> device;
  service->GetDevice(device_info->guid, device.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(contents()->IsConnectedToUsbDevice());

  EXPECT_CALL(web_contents_observer, OnIsConnectedToUsbDeviceChanged(true));
  OpenDeviceBlocking(device.get());
  EXPECT_TRUE(contents()->IsConnectedToUsbDevice());

  base::RunLoop loop;
  EXPECT_CALL(web_contents_observer, OnIsConnectedToUsbDeviceChanged(false))
      .WillOnce([&loop](bool is_connected_to_usb_device) { loop.Quit(); });
  NavigateAndCommit(GURL(kCrossOriginTestUrl));
  loop.Run();
  EXPECT_FALSE(contents()->IsConnectedToUsbDevice());
}

class WebUsbServiceImplProtectedInterfaceTest
    : public WebUsbServiceImplTest,
      public testing::WithParamInterface<uint8_t> {};

TEST_P(WebUsbServiceImplProtectedInterfaceTest, BlockProtectedInterface) {
  const auto kOrigin = url::Origin::Create(GURL(kDefaultTestUrl));

  auto blocked_interface_alt = device::mojom::UsbAlternateInterfaceInfo::New();
  blocked_interface_alt->alternate_setting = 0;
  blocked_interface_alt->class_code = GetParam();

  auto blocked_interface = device::mojom::UsbInterfaceInfo::New();
  blocked_interface->interface_number = 0;
  blocked_interface->alternates.push_back(std::move(blocked_interface_alt));

  auto unblocked_interface_alt =
      device::mojom::UsbAlternateInterfaceInfo::New();
  unblocked_interface_alt->alternate_setting = 0;
  unblocked_interface_alt->class_code = 0xff;  // Vendor specific interface.

  auto unblocked_interface = device::mojom::UsbInterfaceInfo::New();
  unblocked_interface->interface_number = 1;
  unblocked_interface->alternates.push_back(std::move(unblocked_interface_alt));

  auto config = device::mojom::UsbConfigurationInfo::New();
  config->configuration_value = 1;
  config->interfaces.push_back(std::move(blocked_interface));
  config->interfaces.push_back(std::move(unblocked_interface));

  std::vector<device::mojom::UsbConfigurationInfoPtr> configs;
  configs.push_back(std::move(config));

  auto fake_device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF", std::move(configs));

  auto device_info = device_manager()->AddDevice(fake_device);

  mojo::Remote<blink::mojom::WebUsbService> service;
  ConnectToService(service.BindNewPipeAndPassReceiver());

  GetDevicesBlocking(service.get(), {device_info->guid});

  mojo::Remote<device::mojom::UsbDevice> device;
  service->GetDevice(device_info->guid, device.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(contents()->IsConnectedToUsbDevice());

  OpenDeviceBlocking(device.get());

  TestFuture<bool> set_configuration_future;
  device->SetConfiguration(1, set_configuration_future.GetCallback());
  EXPECT_TRUE(set_configuration_future.Get());

  TestFuture<device::mojom::UsbClaimInterfaceResult> claim_interface0_future;
  device->ClaimInterface(0, claim_interface0_future.GetCallback());
  EXPECT_EQ(claim_interface0_future.Get(),
            device::mojom::UsbClaimInterfaceResult::kProtectedClass);

  TestFuture<device::mojom::UsbClaimInterfaceResult> claim_interface1_future;
  device->ClaimInterface(1, claim_interface1_future.GetCallback());
  EXPECT_EQ(claim_interface1_future.Get(),
            device::mojom::UsbClaimInterfaceResult::kSuccess);
}

INSTANTIATE_TEST_SUITE_P(WebUsbServiceImplProtectedInterfaceTests,
                         WebUsbServiceImplProtectedInterfaceTest,
                         testing::Values(device::mojom::kUsbAudioClass,
                                         device::mojom::kUsbHidClass,
                                         device::mojom::kUsbMassStorageClass,
                                         device::mojom::kUsbSmartCardClass,
                                         device::mojom::kUsbVideoClass,
                                         device::mojom::kUsbAudioVideoClass,
                                         device::mojom::kUsbWirelessClass));

}  // namespace content
