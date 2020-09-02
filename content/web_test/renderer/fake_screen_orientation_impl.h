// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_FAKE_SCREEN_ORIENTATION_IMPL_H_
#define CONTENT_WEB_TEST_RENDERER_FAKE_SCREEN_ORIENTATION_IMPL_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "services/device/public/mojom/screen_orientation.mojom.h"
#include "services/device/public/mojom/screen_orientation_lock_types.mojom.h"
#include "third_party/blink/public/mojom/widget/screen_orientation.mojom.h"

namespace blink {
class WebLocalFrame;
}

namespace content {
class WebViewTestProxy;

// An implementation of mojom::ScreenOrientation for web tests, that lives in
// the renderer process.
class FakeScreenOrientationImpl : public device::mojom::ScreenOrientation {
 public:
  explicit FakeScreenOrientationImpl();
  ~FakeScreenOrientationImpl() override;

  FakeScreenOrientationImpl(const FakeScreenOrientationImpl&) = delete;
  FakeScreenOrientationImpl& operator=(const FakeScreenOrientationImpl&) =
      delete;

  void ResetData();
  bool UpdateDeviceOrientation(WebViewTestProxy* web_view,
                               blink::mojom::ScreenOrientation orientation);

  base::Optional<blink::mojom::ScreenOrientation> CurrentOrientationType()
      const;
  bool IsDisabled() const { return is_disabled_; }
  void SetDisabled(WebViewTestProxy* web_view, bool disabled);

  void AddReceiver(mojo::ScopedInterfaceEndpointHandle handle);
  void OverrideAssociatedInterfaceProviderForFrame(blink::WebLocalFrame* frame);

  // device::mojom::ScreenOrientation implementation.
  void LockOrientation(device::mojom::ScreenOrientationLockType orientation,
                       LockOrientationCallback callback) override;
  void UnlockOrientation() override;

 private:
  void UpdateLockSync(device::mojom::ScreenOrientationLockType,
                      LockOrientationCallback callback);
  void ResetLockSync();

  bool UpdateScreenOrientation(blink::mojom::ScreenOrientation);
  bool IsOrientationAllowedByCurrentLock(blink::mojom::ScreenOrientation);
  blink::mojom::ScreenOrientation SuitableOrientationForCurrentLock();

  WebViewTestProxy* web_view_test_proxy_ = nullptr;
  device::mojom::ScreenOrientationLockType current_lock_ =
      device::mojom::ScreenOrientationLockType::DEFAULT;
  blink::mojom::ScreenOrientation device_orientation_ =
      blink::mojom::ScreenOrientation::kPortraitPrimary;
  blink::mojom::ScreenOrientation current_orientation_ =
      blink::mojom::ScreenOrientation::kPortraitPrimary;
  bool is_disabled_ = false;
  mojo::AssociatedReceiverSet<device::mojom::ScreenOrientation> receivers_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_FAKE_SCREEN_ORIENTATION_IMPL_H_
