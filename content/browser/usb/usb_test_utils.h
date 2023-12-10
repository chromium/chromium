// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_USB_USB_TEST_UTILS_H_
#define CONTENT_BROWSER_USB_USB_TEST_UTILS_H_

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/observer_list.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/usb_chooser.h"
#include "content/public/browser/usb_delegate.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/usb_device.mojom-forward.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom-forward.h"
#include "services/device/public/mojom/usb_manager.mojom-forward.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "url/origin.h"

namespace content {

class RenderFrameHost;

// A mock UsbDeviceManagerClient implementation that can be used to listen for
// USB device connection events.
class MockDeviceManagerClient : public device::mojom::UsbDeviceManagerClient {
 public:
  MockDeviceManagerClient();
  MockDeviceManagerClient(MockDeviceManagerClient&) = delete;
  MockDeviceManagerClient& operator=(MockDeviceManagerClient&) = delete;
  ~MockDeviceManagerClient() override;

  void Bind(
      mojo::PendingAssociatedReceiver<device::mojom::UsbDeviceManagerClient>
          receiver) {
    receiver_.Bind(std::move(receiver));
  }

  mojo::PendingAssociatedRemote<device::mojom::UsbDeviceManagerClient>
  CreateInterfacePtrAndBind() {
    auto client = receiver_.BindNewEndpointAndPassRemote();
    receiver_.set_disconnect_handler(base::BindOnce(
        &MockDeviceManagerClient::OnConnectionError, base::Unretained(this)));
    return client;
  }

  MOCK_METHOD(void, OnDeviceAdded, (device::mojom::UsbDeviceInfoPtr));
  MOCK_METHOD(void, OnDeviceRemoved, (device::mojom::UsbDeviceInfoPtr));

  MOCK_METHOD(void, ConnectionError, ());
  void OnConnectionError() {
    receiver_.reset();
    ConnectionError();
  }

 private:
  mojo::AssociatedReceiver<device::mojom::UsbDeviceManagerClient> receiver_{
      this};
};

// A UsbDelegate implementation that can be mocked for tests.
class MockUsbDelegate : public UsbDelegate {
 public:
  MockUsbDelegate();
  MockUsbDelegate(MockUsbDelegate&) = delete;
  MockUsbDelegate& operator=(MockUsbDelegate&) = delete;
  ~MockUsbDelegate() override;

  // Simulates opening the USB device chooser dialog and selecting an item. The
  // chooser automatically selects the item returned by RunChooserInternal,
  // which may be mocked. Returns `nullptr`. `options` is ignored.
  std::unique_ptr<UsbChooser> RunChooser(
      RenderFrameHost& frame,
      blink::mojom::WebUsbRequestDeviceOptionsPtr options,
      blink::mojom::WebUsbService::GetPermissionCallback callback) override;

  void AddObserver(BrowserContext* browser_context,
                   Observer* observer) override;
  void RemoveObserver(BrowserContext* browser_context,
                      Observer* observer) override;

  // Simulate events from tests.
  void OnDeviceAdded(const device::mojom::UsbDeviceInfo& device);
  void OnDeviceRemoved(const device::mojom::UsbDeviceInfo& device);
  void OnPermissionRevoked(const url::Origin& origin);
  void OnDeviceManagerConnectionError();

  MOCK_METHOD0(RunChooserInternal, device::mojom::UsbDeviceInfoPtr());
  MOCK_METHOD4(AdjustProtectedInterfaceClasses,
               void(BrowserContext*,
                    const url::Origin&,
                    RenderFrameHost*,
                    std::vector<uint8_t>&));
  MOCK_METHOD1(PageMayUseUsb, bool(Page&));
  MOCK_METHOD2(CanRequestDevicePermission,
               bool(BrowserContext*, const url::Origin&));
  MOCK_METHOD3(RevokeDevicePermissionWebInitiated,
               void(BrowserContext*,
                    const url::Origin&,
                    const device::mojom::UsbDeviceInfo&));
  MOCK_METHOD2(GetDeviceInfo,
               const device::mojom::UsbDeviceInfo*(BrowserContext*,
                                                   const std::string& guid));
  MOCK_METHOD4(HasDevicePermission,
               bool(BrowserContext*,
                    RenderFrameHost*,
                    const url::Origin&,
                    const device::mojom::UsbDeviceInfo&));
  MOCK_METHOD2(GetDevices,
               void(BrowserContext*,
                    blink::mojom::WebUsbService::GetDevicesCallback));
  MOCK_METHOD5(GetDevice,
               void(BrowserContext*,
                    const std::string&,
                    base::span<const uint8_t>,
                    mojo::PendingReceiver<device::mojom::UsbDevice>,
                    mojo::PendingRemote<device::mojom::UsbDeviceClient>));
  MOCK_METHOD2(SetDeviceManagerForTesting,
               void(RenderFrameHost&,
                    mojo::PendingRemote<device::mojom::UsbDeviceManager>
                        device_manager));
  MOCK_METHOD1(IsServiceWorkerAllowedForOrigin, bool(const url::Origin&));
  MOCK_METHOD2(IncrementConnectionCount,
               void(BrowserContext*, const url::Origin&));
  MOCK_METHOD2(DecrementConnectionCount,
               void(BrowserContext*, const url::Origin&));

  const base::ObserverList<Observer>& observer_list() { return observer_list_; }

  void SetAssertBrowserContext(bool assert_browser_context);

 private:
  base::ObserverList<UsbDelegate::Observer> observer_list_;
  bool assert_browser_context_ = false;
};

template <typename SuperClass>
class UsbTestContentBrowserClientBase : public SuperClass {
 public:
  MockUsbDelegate& delegate() { return delegate_; }

  // ContentBrowserClient:
  UsbDelegate* GetUsbDelegate() override { return &delegate_; }

 private:
  testing::NiceMock<MockUsbDelegate> delegate_;
};

using UsbTestContentBrowserClient =
    UsbTestContentBrowserClientBase<ContentBrowserClient>;

}  // namespace content

#endif  // CONTENT_BROWSER_USB_USB_TEST_UTILS_H_
