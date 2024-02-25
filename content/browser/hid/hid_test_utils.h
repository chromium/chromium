// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HID_HID_TEST_UTILS_H_
#define CONTENT_BROWSER_HID_HID_TEST_UTILS_H_

#include <memory>
#include <vector>

#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/hid_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/device/public/mojom/hid.mojom-forward.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/hid/hid.mojom-forward.h"
#include "url/origin.h"

namespace content {

// Mock HidDelegate implementation for tests that need to simulate permissions
// management for HID device access.
class MockHidDelegate : public HidDelegate {
 public:
  MockHidDelegate();
  MockHidDelegate(MockHidDelegate&) = delete;
  MockHidDelegate& operator=(MockHidDelegate&) = delete;
  ~MockHidDelegate() override;

  // Simulates opening the HID device chooser dialog and selecting an item. The
  // chooser automatically selects the item returned by RunChooserInternal,
  // which may be mocked. Returns nullptr. Device filters and exclusion_filters
  // are ignored.
  std::unique_ptr<HidChooser> RunChooser(
      RenderFrameHost* frame,
      std::vector<blink::mojom::HidDeviceFilterPtr> filters,
      std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters,
      HidChooser::Callback callback) override;

  void AddObserver(BrowserContext* browser_context,
                   Observer* observer) override;
  void RemoveObserver(BrowserContext* browser_context,
                      Observer* observer) override;

  // MockHidDelegate does not register to receive device connection events. Use
  // these methods to broadcast device connections to all delegate observers.
  void OnDeviceAdded(const device::mojom::HidDeviceInfo& device);
  void OnDeviceRemoved(const device::mojom::HidDeviceInfo& device);
  void OnDeviceChanged(const device::mojom::HidDeviceInfo& device);
  void OnPermissionRevoked(const url::Origin& origin);
  void OnHidManagerConnectionError();

  MOCK_METHOD0(RunChooserInternal,
               std::vector<device::mojom::HidDeviceInfoPtr>());
  MOCK_METHOD2(CanRequestDevicePermission,
               bool(BrowserContext* browser_context,
                    const url::Origin& origin));
  MOCK_METHOD4(HasDevicePermission,
               bool(BrowserContext* browser_context,
                    RenderFrameHost* render_frame_host,
                    const url::Origin& origin,
                    const device::mojom::HidDeviceInfo& device));
  MOCK_METHOD4(RevokeDevicePermission,
               void(BrowserContext* browser_context,
                    RenderFrameHost* render_frame_host,
                    const url::Origin& origin,
                    const device::mojom::HidDeviceInfo& device));
  MOCK_METHOD1(GetHidManager,
               device::mojom::HidManager*(BrowserContext* browser_context));
  MOCK_METHOD2(
      GetDeviceInfo,
      const device::mojom::HidDeviceInfo*(BrowserContext* browser_context,
                                          const std::string& guid));
  MOCK_METHOD2(IsFidoAllowedForOrigin,
               bool(BrowserContext* browser_context,
                    const url::Origin& origin));
  MOCK_METHOD1(IsServiceWorkerAllowedForOrigin,
               bool(const url::Origin& origin));
  MOCK_METHOD2(IncrementConnectionCount,
               void(BrowserContext*, const url::Origin&));
  MOCK_METHOD2(DecrementConnectionCount,
               void(BrowserContext*, const url::Origin&));

  const base::ObserverList<Observer>& observer_list() { return observer_list_; }

  void SetAssertBrowserContext(bool assert_browser_context);

 private:
  base::ObserverList<Observer> observer_list_;
  bool assert_browser_context_ = false;
};

// Test implementation of ContentBrowserClient for HID tests. The test client
// also provides direct access to the MockHidDelegate.
class HidTestContentBrowserClient : public ContentBrowserClient {
 public:
  HidTestContentBrowserClient();
  HidTestContentBrowserClient(HidTestContentBrowserClient&) = delete;
  HidTestContentBrowserClient& operator=(HidTestContentBrowserClient&) = delete;
  ~HidTestContentBrowserClient() override;

  MockHidDelegate& delegate() { return delegate_; }

  // ContentBrowserClient:
  HidDelegate* GetHidDelegate() override;

 private:
  MockHidDelegate delegate_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_HID_HID_TEST_UTILS_H_
