// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/hid/hid_test_utils.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"

#include <utility>

#include "base/functional/callback.h"

namespace content {

MockHidDelegate::MockHidDelegate() = default;

MockHidDelegate::~MockHidDelegate() = default;

std::unique_ptr<HidChooser> MockHidDelegate::RunChooser(
    RenderFrameHost* frame,
    std::vector<blink::mojom::HidDeviceFilterPtr> filters,
    std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters,
    HidChooser::Callback callback) {
  std::move(callback).Run(RunChooserInternal());
  return nullptr;
}

void MockHidDelegate::AddObserver(BrowserContext* browser_context,
                                  Observer* observer) {
  if (assert_browser_context_) {
    ASSERT_TRUE(browser_context);
  }
  observer_list_.AddObserver(observer);
}

void MockHidDelegate::RemoveObserver(BrowserContext* browser_context,
                                     Observer* observer) {
  if (assert_browser_context_) {
    ASSERT_TRUE(browser_context);
  }
  observer_list_.RemoveObserver(observer);
}

void MockHidDelegate::OnDeviceAdded(
    const device::mojom::HidDeviceInfo& device) {
  for (auto& observer : observer_list_)
    observer.OnDeviceAdded(device);
}

void MockHidDelegate::OnDeviceRemoved(
    const device::mojom::HidDeviceInfo& device) {
  for (auto& observer : observer_list_)
    observer.OnDeviceRemoved(device);
}

void MockHidDelegate::OnDeviceChanged(
    const device::mojom::HidDeviceInfo& device) {
  for (auto& observer : observer_list_)
    observer.OnDeviceChanged(device);
}

void MockHidDelegate::OnPermissionRevoked(const url::Origin& origin) {
  for (auto& observer : observer_list_)
    observer.OnPermissionRevoked(origin);
}

void MockHidDelegate::OnHidManagerConnectionError() {
  for (auto& observer : observer_list_) {
    observer.OnHidManagerConnectionError();
  }
}

void MockHidDelegate::SetAssertBrowserContext(bool assert_browser_context) {
  assert_browser_context_ = assert_browser_context;
}

HidTestContentBrowserClient::HidTestContentBrowserClient() = default;

HidTestContentBrowserClient::~HidTestContentBrowserClient() = default;

HidDelegate* HidTestContentBrowserClient::GetHidDelegate() {
  return &delegate_;
}

}  // namespace content
