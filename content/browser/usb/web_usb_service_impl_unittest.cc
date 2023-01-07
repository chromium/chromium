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
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/usb/usb_test_utils.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_contents_factory.h"
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
using ::testing::Combine;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::TestParamInfo;
using ::testing::Values;
using ::testing::WithParamInterface;

enum ServiceCreationType {
  kCreateForFrame,
  kCreateForServiceWorker,
};

constexpr base::StringPiece kDefaultTestUrl{"https://www.google.com/"};
constexpr base::StringPiece kCrossOriginTestUrl{"https://www.chromium.org"};

MATCHER_P(HasGuid, matcher, "") {
  return ExplainMatchResult(matcher, arg->guid, result_listener);
}

std::string ServiceCreationTypeToString(ServiceCreationType type) {
  switch (type) {
    case kCreateForFrame:
      return "Frame";
    case kCreateForServiceWorker:
      return "ServiceWorker";
  }
}

std::string ClassCodeToString(uint8_t class_code) {
  switch (class_code) {
    case device::mojom::kUsbAudioClass:
      return "AudioClass";
    case device::mojom::kUsbHidClass:
      return "HidClass";
    case device::mojom::kUsbMassStorageClass:
      return "MassStorageClass";
    case device::mojom::kUsbSmartCardClass:
      return "SmartCardClass";
    case device::mojom::kUsbVideoClass:
      return "VideoClass";
    case device::mojom::kUsbAudioVideoClass:
      return "AudioVideoClass";
    case device::mojom::kUsbWirelessClass:
      return "WirelessClass";
    default:
      return "UnknownClass";
  }
}

}  // namespace

class WebUsbServiceImplBaseTest : public testing::Test {
 public:
  WebUsbServiceImplBaseTest() = default;
  WebUsbServiceImplBaseTest(WebUsbServiceImplBaseTest&) = delete;
  WebUsbServiceImplBaseTest& operator=(WebUsbServiceImplBaseTest&) = delete;
  ~WebUsbServiceImplBaseTest() override = default;

  void SetUp() override {
    // Connect with the FakeUsbDeviceManager.
    mojo::PendingRemote<device::mojom::UsbDeviceManager> pending_device_manager;
    device_manager_.AddReceiver(
        pending_device_manager.InitWithNewPipeAndPassReceiver());

    original_client_ = SetBrowserClientForTesting(&test_client_);

    // For tests, all devices are permitted by default.
    ON_CALL(delegate(), HasDevicePermission).WillByDefault(Return(true));

    // Forward calls to the fake device manager.
    ON_CALL(delegate(), GetDevices)
        .WillByDefault(
            [this](
                auto* browser_context,
                device::mojom::UsbDeviceManager::GetDevicesCallback callback) {
              device_manager_.GetDevices(nullptr, std::move(callback));
            });
    ON_CALL(delegate(), GetDevice)
        .WillByDefault(
            [this](
                auto* browser_context, const std::string& guid,
                base::span<const uint8_t> blocked_interface_classes,
                mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
                mojo::PendingRemote<device::mojom::UsbDeviceClient>
                    device_client) {
              device_manager_.GetDevice(
                  guid,
                  std::vector<uint8_t>(blocked_interface_classes.begin(),
                                       blocked_interface_classes.end()),
                  std::move(device_receiver), std::move(device_client));
            });
    ON_CALL(delegate(), GetDeviceInfo)
        .WillByDefault([this](auto* browser_context, const std::string& guid) {
          return device_manager_.GetDeviceInfo(guid);
        });
  }

  void TearDown() override { SetBrowserClientForTesting(original_client_); }

  const mojo::Remote<blink::mojom::WebUsbService>& GetService(
      ServiceCreationType type) {
    switch (type) {
      case kCreateForFrame:
        web_contents_ =
            web_contents_factory_.CreateWebContents(&browser_context_);
        contents()->NavigateAndCommit(GURL(kDefaultTestUrl));
        contents()->GetPrimaryMainFrame()->CreateWebUsbService(
            service_.BindNewPipeAndPassReceiver());
        break;
      case kCreateForServiceWorker:
        embedded_worker_test_helper_ =
            std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());
        EXPECT_CALL(delegate(), IsServiceWorkerAllowedForOrigin)
            .WillOnce(Return(true));
        WebUsbServiceImpl::Create(
            embedded_worker_test_helper_->context()->AsWeakPtr(),
            url::Origin::Create(GURL(kDefaultTestUrl)),
            service_.BindNewPipeAndPassReceiver());
        break;
    }
    return service_;
  }

  device::mojom::UsbDeviceInfoPtr ConnectDevice(
      scoped_refptr<device::FakeUsbDeviceInfo> device,
      device::MockUsbMojoDevice* mock_device) {
    auto device_info = device_manager_.AddDevice(std::move(device));
    if (mock_device)
      device_manager_.SetMockForDevice(device_info->guid, mock_device);
    delegate().OnDeviceAdded(*device_info);
    return device_info;
  }

  void DisconnectDevice(scoped_refptr<device::FakeUsbDeviceInfo> device) {
    auto device_info = device->GetDeviceInfo().Clone();
    device_manager_.RemoveDevice(std::move(device));
    delegate().OnDeviceRemoved(*device_info);
  }

  scoped_refptr<device::FakeUsbDeviceInfo> CreateFakeDevice() {
    return base::MakeRefCounted<device::FakeUsbDeviceInfo>(
        0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  }

  scoped_refptr<device::FakeUsbDeviceInfo>
  CreateFakeDeviceWithProtectedInterface(uint8_t class_code) {
    auto blocked_interface_alt =
        device::mojom::UsbAlternateInterfaceInfo::New();
    blocked_interface_alt->alternate_setting = 0;
    blocked_interface_alt->class_code = class_code;

    auto blocked_interface = device::mojom::UsbInterfaceInfo::New();
    blocked_interface->interface_number = 0;
    blocked_interface->alternates.push_back(std::move(blocked_interface_alt));

    auto unblocked_interface_alt =
        device::mojom::UsbAlternateInterfaceInfo::New();
    unblocked_interface_alt->alternate_setting = 0;
    unblocked_interface_alt->class_code = 0xff;  // Vendor specific interface.

    auto unblocked_interface = device::mojom::UsbInterfaceInfo::New();
    unblocked_interface->interface_number = 1;
    unblocked_interface->alternates.push_back(
        std::move(unblocked_interface_alt));

    auto config = device::mojom::UsbConfigurationInfo::New();
    config->configuration_value = 1;
    config->interfaces.push_back(std::move(blocked_interface));
    config->interfaces.push_back(std::move(unblocked_interface));

    std::vector<device::mojom::UsbConfigurationInfoPtr> configs;
    configs.push_back(std::move(config));

    return base::MakeRefCounted<device::FakeUsbDeviceInfo>(
        0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF", std::move(configs));
  }

  void SimulateDeviceServiceCrash() { device_manager_.CloseAllBindings(); }
  void CheckIsConnected(bool expected_is_connected) {
    // Skip the check for service workers which do not have web contents.
    if (!web_contents_)
      return;

    EXPECT_EQ(expected_is_connected, web_contents_->IsConnectedToUsbDevice());
  }

  void DestroyBrowserContext() { embedded_worker_test_helper_.reset(); }

  TestWebContents* contents() {
    return static_cast<TestWebContents*>(web_contents_);
  }

  MockUsbDelegate& delegate() { return test_client_.delegate(); }

 private:
  BrowserTaskEnvironment task_environment_;
  mojo::Remote<blink::mojom::WebUsbService> service_;
  device::FakeUsbDeviceManager device_manager_;
  UsbTestContentBrowserClient test_client_;
  raw_ptr<ContentBrowserClient> original_client_;
  TestBrowserContext browser_context_;
  TestWebContentsFactory web_contents_factory_;
  raw_ptr<WebContents> web_contents_ = nullptr;
  std::unique_ptr<EmbeddedWorkerTestHelper> embedded_worker_test_helper_;
};

class WebUsbServiceImplTest : public WebUsbServiceImplBaseTest,
                              public WithParamInterface<ServiceCreationType> {};

TEST_P(WebUsbServiceImplTest, OpenAndCloseDevice) {
  const auto origin = url::Origin::Create(GURL(kDefaultTestUrl));

  const auto service_creation_type = GetParam();
  const auto& service = GetService(service_creation_type);
  NiceMock<MockWebContentsObserver> web_contents_observer(contents());

  device::MockUsbMojoDevice mock_device;
  auto device_info = ConnectDevice(CreateFakeDevice(), &mock_device);

  TestFuture<std::vector<device::mojom::UsbDeviceInfoPtr>> devices_future;
  service->GetDevices(devices_future.GetCallback());
  EXPECT_THAT(devices_future.Get(), ElementsAre(HasGuid(device_info->guid)));

  mojo::Remote<device::mojom::UsbDevice> device;
  service->GetDevice(device_info->guid, device.BindNewPipeAndPassReceiver());
  CheckIsConnected(false);

  EXPECT_CALL(web_contents_observer, OnIsConnectedToUsbDeviceChanged(true))
      .Times(service_creation_type == kCreateForFrame ? 1 : 0);
  EXPECT_CALL(mock_device, Open)
      .WillOnce(RunOnceCallback<0>(device::mojom::UsbOpenDeviceError::OK));
  TestFuture<device::mojom::UsbOpenDeviceError> open_future;
  device->Open(open_future.GetCallback());
  EXPECT_EQ(open_future.Get(), device::mojom::UsbOpenDeviceError::OK);
  CheckIsConnected(true);

  EXPECT_CALL(web_contents_observer, OnIsConnectedToUsbDeviceChanged(false))
      .Times(service_creation_type == kCreateForFrame ? 1 : 0);
  EXPECT_CALL(mock_device, Close).WillOnce(RunOnceClosure<0>());
  base::RunLoop run_loop;
  device->Close(run_loop.QuitClosure());
  run_loop.Run();
  CheckIsConnected(false);
}

TEST_P(WebUsbServiceImplTest, OpenAndDisconnectDevice) {
  const auto service_creation_type = GetParam();
  const auto origin = url::Origin::Create(GURL(kDefaultTestUrl));

  const auto& service = GetService(service_creation_type);
  NiceMock<MockWebContentsObserver> web_contents_observer(contents());

  device::MockUsbMojoDevice mock_device;
  auto fake_device_info = CreateFakeDevice();
  auto device_info = ConnectDevice(fake_device_info, &mock_device);

  TestFuture<std::vector<device::mojom::UsbDeviceInfoPtr>> devices_future;
  service->GetDevices(devices_future.GetCallback());
  EXPECT_THAT(devices_future.Get(), ElementsAre(HasGuid(device_info->guid)));

  mojo::Remote<device::mojom::UsbDevice> device;
  service->GetDevice(device_info->guid, device.BindNewPipeAndPassReceiver());
  CheckIsConnected(false);

  EXPECT_CALL(web_contents_observer, OnIsConnectedToUsbDeviceChanged(true))
      .Times(service_creation_type == kCreateForFrame ? 1 : 0);
  EXPECT_CALL(mock_device, Open)
      .WillOnce(RunOnceCallback<0>(device::mojom::UsbOpenDeviceError::OK));
  TestFuture<device::mojom::UsbOpenDeviceError> open_future;
  device->Open(open_future.GetCallback());
  EXPECT_EQ(open_future.Get(), device::mojom::UsbOpenDeviceError::OK);
  CheckIsConnected(true);

  base::RunLoop loop;
  EXPECT_CALL(mock_device, Close).WillOnce([&]() { loop.Quit(); });
  EXPECT_CALL(web_contents_observer, OnIsConnectedToUsbDeviceChanged(false))
      .Times(service_creation_type == kCreateForFrame ? 1 : 0);
  DisconnectDevice(fake_device_info);
  loop.Run();
  CheckIsConnected(false);
}

INSTANTIATE_TEST_SUITE_P(WebUsbServiceImplTests,
                         WebUsbServiceImplTest,
                         Values(kCreateForFrame, kCreateForServiceWorker),
                         [](const auto& info) {
                           return ServiceCreationTypeToString(info.param);
                         });

using WebUsbServiceImplFrameTest = WebUsbServiceImplBaseTest;

TEST_F(WebUsbServiceImplFrameTest, OpenAndNavigateCrossOrigin) {
  const auto origin = url::Origin::Create(GURL(kDefaultTestUrl));

  const auto& service = GetService(kCreateForFrame);
  NiceMock<MockWebContentsObserver> web_contents_observer(contents());

  device::MockUsbMojoDevice mock_device;
  auto device_info = ConnectDevice(CreateFakeDevice(), &mock_device);

  TestFuture<std::vector<device::mojom::UsbDeviceInfoPtr>> devices_future;
  service->GetDevices(devices_future.GetCallback());
  EXPECT_THAT(devices_future.Get(), ElementsAre(HasGuid(device_info->guid)));

  mojo::Remote<device::mojom::UsbDevice> device;
  service->GetDevice(device_info->guid, device.BindNewPipeAndPassReceiver());
  CheckIsConnected(false);

  EXPECT_CALL(web_contents_observer, OnIsConnectedToUsbDeviceChanged(true));
  EXPECT_CALL(mock_device, Open)
      .WillOnce(RunOnceCallback<0>(device::mojom::UsbOpenDeviceError::OK));
  TestFuture<device::mojom::UsbOpenDeviceError> open_future;
  device->Open(open_future.GetCallback());
  EXPECT_EQ(open_future.Get(), device::mojom::UsbOpenDeviceError::OK);
  CheckIsConnected(true);

  base::RunLoop loop;
  EXPECT_CALL(mock_device, Close).WillOnce([&]() { loop.Quit(); });
  EXPECT_CALL(web_contents_observer, OnIsConnectedToUsbDeviceChanged(false));
  contents()->NavigateAndCommit(GURL(kCrossOriginTestUrl));
  loop.Run();
  CheckIsConnected(false);
}

class WebUsbServiceImplProtectedInterfaceTest
    : public WebUsbServiceImplBaseTest,
      public WithParamInterface<std::tuple<ServiceCreationType, uint8_t>> {};

TEST_P(WebUsbServiceImplProtectedInterfaceTest, BlockProtectedInterface) {
  const auto service_creation_type = std::get<0>(GetParam());
  const auto class_code = std::get<1>(GetParam());

  auto device_info =
      ConnectDevice(CreateFakeDeviceWithProtectedInterface(class_code),
                    /*mock_device=*/nullptr);

  const auto& service = GetService(service_creation_type);
  TestFuture<std::vector<device::mojom::UsbDeviceInfoPtr>> devices_future;
  service->GetDevices(devices_future.GetCallback());
  EXPECT_THAT(devices_future.Get(), ElementsAre(HasGuid(device_info->guid)));

  mojo::Remote<device::mojom::UsbDevice> device;
  service->GetDevice(device_info->guid, device.BindNewPipeAndPassReceiver());

  TestFuture<device::mojom::UsbOpenDeviceError> open_future;
  device->Open(open_future.GetCallback());
  EXPECT_EQ(open_future.Get(), device::mojom::UsbOpenDeviceError::OK);

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

INSTANTIATE_TEST_SUITE_P(
    WebUsbServiceImplProtectedInterfaceTests,
    WebUsbServiceImplProtectedInterfaceTest,
    Combine(Values(kCreateForFrame, kCreateForServiceWorker),
            Values(device::mojom::kUsbAudioClass,
                   device::mojom::kUsbHidClass,
                   device::mojom::kUsbMassStorageClass,
                   device::mojom::kUsbSmartCardClass,
                   device::mojom::kUsbVideoClass,
                   device::mojom::kUsbAudioVideoClass,
                   device::mojom::kUsbWirelessClass)),
    [](const TestParamInfo<std::tuple<ServiceCreationType, uint8_t>>& info) {
      return base::StringPrintf(
          "%s_%s", ServiceCreationTypeToString(std::get<0>(info.param)).c_str(),
          ClassCodeToString(std::get<1>(info.param)).c_str());
    });

}  // namespace content
