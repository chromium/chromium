// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "content/browser/usb/usb_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/usb_chooser.h"
#include "content/public/browser/usb_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "url/origin.h"

namespace content {

namespace {

using ::testing::Return;

class WebUsbTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    test_client_ = std::make_unique<UsbTestContentBrowserClientBase<
        ContentBrowserTestContentBrowserClient>>();

    AddFakeDevice("123456");

    // Connect with the FakeUsbDeviceManager.
    mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager;
    device_manager_.AddReceiver(
        device_manager.InitWithNewPipeAndPassReceiver());

    // The chooser will always select the last-created `fake_device_info_`.
    EXPECT_CALL(delegate(), RunChooserInternal).WillRepeatedly([&]() {
      permission_granted_ = true;
      return fake_device_info_.Clone();
    });
    EXPECT_CALL(delegate(), HasDevicePermission).WillRepeatedly([&]() {
      return permission_granted_;
    });
    EXPECT_CALL(delegate(), RevokeDevicePermissionWebInitiated)
        .WillRepeatedly([&]() { permission_granted_ = false; });

    // All origins can request device permissions.
    EXPECT_CALL(delegate(), CanRequestDevicePermission)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(delegate(), PageMayUseUsb).WillRepeatedly(Return(true));

    // Route calls to the FakeUsbDeviceManager.
    EXPECT_CALL(delegate(), GetDeviceInfo)
        .WillRepeatedly([&](auto* browser_context, const std::string& guid) {
          return device_manager_.GetDeviceInfo(guid);
        });
    EXPECT_CALL(delegate(), GetDevices)
        .WillRepeatedly([&](auto* browser_context, auto callback) {
          device_manager_.GetDevices(nullptr, std::move(callback));
        });
    EXPECT_CALL(delegate(), GetDevice)
        .WillRepeatedly(
            [&](auto* browser_context, const std::string& guid,
                base::span<const uint8_t> bic_span,
                mojo::PendingReceiver<device::mojom::UsbDevice> receiver,
                mojo::PendingRemote<device::mojom::UsbDeviceClient> client) {
              std::vector<uint8_t> blocked_interface_classes(bic_span.begin(),
                                                             bic_span.end());
              return device_manager_.GetDevice(guid, blocked_interface_classes,
                                               std::move(receiver),
                                               std::move(client));
            });

    GURL url = embedded_test_server()->GetURL("localhost", "/simple_page.html");
    ASSERT_TRUE(NavigateToURL(shell(), url));
    origin_ = url::Origin::Create(url);

    RenderFrameHost* render_frame_host =
        shell()->web_contents()->GetPrimaryMainFrame();
    EXPECT_EQ(origin_, render_frame_host->GetLastCommittedOrigin());
  }

  void TearDownOnMainThread() override { test_client_.reset(); }

  void AddFakeDevice(const std::string& serial_number) {
    DCHECK(!fake_device_info_);
    fake_device_info_ = device_manager_.CreateAndAddDevice(
        0, 0, "Test Manufacturer", "Test Device", serial_number);
    delegate().OnDeviceAdded(*fake_device_info_);
  }

  void RemoveFakeDevice() {
    DCHECK(fake_device_info_);
    device_manager_.RemoveDevice(fake_device_info_->guid);
    delegate().OnDeviceRemoved(*fake_device_info_);
    fake_device_info_ = nullptr;
  }

  MockUsbDelegate& delegate() { return test_client_->delegate(); }

  WebContents* web_contents() { return shell()->web_contents(); }

 private:
  std::unique_ptr<
      UsbTestContentBrowserClientBase<ContentBrowserTestContentBrowserClient>>
      test_client_;
  device::FakeUsbDeviceManager device_manager_;
  device::mojom::UsbDeviceInfoPtr fake_device_info_;
  bool permission_granted_ = false;
  url::Origin origin_;
};

IN_PROC_BROWSER_TEST_F(WebUsbTest, RequestAndGetDevices) {
  // Call getDevices with no device permissions.
  EXPECT_EQ(ListValueOf(), EvalJs(web_contents(),
                                  R"((async () => {
        let devices = await navigator.usb.getDevices();
        return devices.map(device => device.serialNumber);
      })())"));

  // Request device permissions. The chooser will automatically select the item,
  // granting the permission.
  EXPECT_EQ("123456", EvalJs(web_contents(),
                             R"((async () => {
        let device =
            await navigator.usb.requestDevice({ filters: [{ vendorId: 0 }] });
        return device.serialNumber;
      })())"));

  // Call getDevices again with the device permission granted.
  EXPECT_EQ(ListValueOf("123456"), EvalJs(web_contents(),
                                          R"((async () => {
        let devices = await navigator.usb.getDevices();
        return devices.map(device => device.serialNumber);
      })())"));
}

IN_PROC_BROWSER_TEST_F(WebUsbTest, RequestDeviceWithGuardBlocked) {
  EXPECT_CALL(delegate(), CanRequestDevicePermission).WillOnce(Return(false));
  EXPECT_EQ(
      "NotFoundError: Failed to execute 'requestDevice' on 'USB': No device "
      "selected.",
      EvalJs(web_contents(),
             R"((async () => {
            try {
              await navigator.usb.requestDevice({ filters: [{ vendorId: 0 }] });
              return "Expected error, got success.";
            } catch (e) {
              return `${e.name}: ${e.message}`;
            }
          })())"));
}

IN_PROC_BROWSER_TEST_F(WebUsbTest, AddRemoveDevice) {
  // Request permission to access the fake device.
  EXPECT_EQ("123456", EvalJs(web_contents(),
                             R"((async () => {
        let device =
            await navigator.usb.requestDevice({ filters: [{ vendorId: 0 }] });
        return device.serialNumber;
      })())"));

  EXPECT_TRUE(ExecJs(web_contents(),
                     R"(
        var removedPromise = new Promise(resolve => {
          navigator.usb.addEventListener('disconnect', e => {
            resolve(e.device.serialNumber);
          }, { once: true });
        });
      )"));

  // Disconnect the device. A disconnect event is fired.
  RemoveFakeDevice();
  EXPECT_EQ("123456", EvalJs(web_contents(), "removedPromise"));

  EXPECT_TRUE(ExecJs(web_contents(),
                     R"(
        var addedPromise = new Promise(resolve => {
          navigator.usb.addEventListener('connect', e => {
            resolve(e.device.serialNumber);
          }, { once: true });
        });
      )"));

  // Reconnect the device. A connect event is fired.
  AddFakeDevice("123456");
  EXPECT_EQ("123456", EvalJs(web_contents(), "addedPromise"));
}

IN_PROC_BROWSER_TEST_F(WebUsbTest, ForgetDevice) {
  EXPECT_EQ("123456", EvalJs(web_contents(),
                             R"((async () => {
        let device =
            await navigator.usb.requestDevice({ filters: [{ vendorId: 0 }] });
        return device.serialNumber;
      })())"));

  EXPECT_EQ(1, EvalJs(web_contents(),
                      R"((async () => {
        const devices = await navigator.usb.getDevices();
        return devices.length;
      })())"));

  EXPECT_TRUE(ExecJs(web_contents(),
                     R"((async () => {
        const [device] = await navigator.usb.getDevices();
        await device.forget();
      })())"));

  EXPECT_EQ(0, EvalJs(web_contents(),
                      R"((async () => {
        const devices = await navigator.usb.getDevices();
        return devices.length;
      })())"));
}

}  // namespace

}  // namespace content
