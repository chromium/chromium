// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/usb/usb_test_utils.h"

#include "base/functional/callback.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"

namespace content {

MockDeviceManagerClient::MockDeviceManagerClient() = default;

MockDeviceManagerClient::~MockDeviceManagerClient() = default;

MockUsbDelegate::MockUsbDelegate() = default;

MockUsbDelegate::~MockUsbDelegate() = default;

std::unique_ptr<UsbChooser> MockUsbDelegate::RunChooser(
    RenderFrameHost& frame,
    blink::mojom::WebUsbRequestDeviceOptionsPtr options,
    blink::mojom::WebUsbService::GetPermissionCallback callback) {
  std::move(callback).Run(RunChooserInternal());
  return nullptr;
}

void MockUsbDelegate::AddObserver(BrowserContext* browser_context,
                                  Observer* observer) {
  if (assert_browser_context_) {
    ASSERT_TRUE(browser_context);
  }
  observer_list_.AddObserver(observer);
}

void MockUsbDelegate::RemoveObserver(BrowserContext* browser_context,
                                     Observer* observer) {
  if (assert_browser_context_) {
    ASSERT_TRUE(browser_context);
  }
  observer_list_.RemoveObserver(observer);
}

void MockUsbDelegate::OnDeviceAdded(
    const device::mojom::UsbDeviceInfo& device) {
  for (auto& observer : observer_list_)
    observer.OnDeviceAdded(device);
}

void MockUsbDelegate::OnDeviceRemoved(
    const device::mojom::UsbDeviceInfo& device) {
  for (auto& observer : observer_list_)
    observer.OnDeviceRemoved(device);
}

void MockUsbDelegate::OnPermissionRevoked(const url::Origin& origin) {
  for (auto& observer : observer_list_)
    observer.OnPermissionRevoked(origin);
}

void MockUsbDelegate::OnDeviceManagerConnectionError() {
  for (auto& observer : observer_list_) {
    observer.OnDeviceManagerConnectionError();
  }
}

void MockUsbDelegate::SetAssertBrowserContext(bool assert_browser_context) {
  assert_browser_context_ = assert_browser_context;
}

}  // namespace content
