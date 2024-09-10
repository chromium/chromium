// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_FAKE_SCREEN_ORIENTATION_IMPL_H_
#define CONTENT_WEB_TEST_RENDERER_FAKE_SCREEN_ORIENTATION_IMPL_H_

#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "services/device/public/mojom/screen_orientation.mojom.h"
#include "services/device/public/mojom/screen_orientation_lock_types.mojom.h"
#include "third_party/blink/public/web/web_view_observer.h"
#include "ui/display/mojom/screen_orientation.mojom.h"

namespace blink {
class WebLocalFrame;
class WebView;
}

namespace content {

// An implementation of mojom::ScreenOrientation for web tests, that lives in
// the renderer process.
class FakeScreenOrientationImpl : public device::mojom::ScreenOrientation,
                                  blink::WebViewObserver {
 public:
  explicit FakeScreenOrientationImpl();
  ~FakeScreenOrientationImpl() override;

  FakeScreenOrientationImpl(const FakeScreenOrientationImpl&) = delete;
  FakeScreenOrientationImpl& operator=(const FakeScreenOrientationImpl&) =
      delete;

  void ResetData();
  bool UpdateDeviceOrientation(blink::WebView* web_view,
                               display::mojom::ScreenOrientation orientation);

  std::optional<display::mojom::ScreenOrientation> CurrentOrientationType()
      const;
  bool IsDisabled() const { return is_disabled_; }
  void SetDisabled(blink::WebView* web_view, bool disabled);

  void AddReceiver(mojo::ScopedInterfaceEndpointHandle handle);
  void OverrideAssociatedInterfaceProviderForFrame(blink::WebLocalFrame* frame);

  // device::mojom::ScreenOrientation implementation.
  void LockOrientation(device::mojom::ScreenOrientationLockType orientation,
                       LockOrientationCallback callback) override;
  void UnlockOrientation() override;

  // WebViewObserver implementation.
  void OnDestruct() override {}

 private:
  void UpdateLockSync(device::mojom::ScreenOrientationLockType,
                      LockOrientationCallback callback);
  void ResetLockSync();

  bool UpdateScreenOrientation(display::mojom::ScreenOrientation);
  bool IsOrientationAllowedByCurrentLock(display::mojom::ScreenOrientation);
  display::mojom::ScreenOrientation SuitableOrientationForCurrentLock();

  device::mojom::ScreenOrientationLockType current_lock_ =
      device::mojom::ScreenOrientationLockType::DEFAULT;
  display::mojom::ScreenOrientation device_orientation_ =
      display::mojom::ScreenOrientation::kPortraitPrimary;
  display::mojom::ScreenOrientation current_orientation_ =
      display::mojom::ScreenOrientation::kPortraitPrimary;
  bool is_disabled_ = false;
  mojo::AssociatedReceiverSet<device::mojom::ScreenOrientation> receivers_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_FAKE_SCREEN_ORIENTATION_IMPL_H_
