// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/usb/usb_test_utils.h"

#include "base/callback.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"

namespace content {

MockUsbDelegate::MockUsbDelegate() = default;

MockUsbDelegate::~MockUsbDelegate() = default;

std::unique_ptr<UsbChooser> MockUsbDelegate::RunChooser(
    RenderFrameHost& frame,
    std::vector<device::mojom::UsbDeviceFilterPtr> filters,
    blink::mojom::WebUsbService::GetPermissionCallback callback) {
  std::move(callback).Run(RunChooserInternal());
  return nullptr;
}

void MockUsbDelegate::AddObserver(BrowserContext* browser_context,
                                  Observer* observer) {
  observer_list_.AddObserver(observer);
}

void MockUsbDelegate::RemoveObserver(BrowserContext* browser_context,
                                     Observer* observer) {
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

UsbTestContentBrowserClient::UsbTestContentBrowserClient() = default;

UsbTestContentBrowserClient::~UsbTestContentBrowserClient() = default;

UsbDelegate* UsbTestContentBrowserClient::GetUsbDelegate() {
  return &delegate_;
}

}  // namespace content
