// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/fake_screen_orientation_impl.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

using blink::WebView;

namespace content {

FakeScreenOrientationImpl::FakeScreenOrientationImpl()
    : WebViewObserver(nullptr) {}

FakeScreenOrientationImpl::~FakeScreenOrientationImpl() = default;

void FakeScreenOrientationImpl::ResetData() {
  Observe(nullptr);
  current_lock_ = device::mojom::ScreenOrientationLockType::DEFAULT;
  device_orientation_ = display::mojom::ScreenOrientation::kPortraitPrimary;
  current_orientation_ = display::mojom::ScreenOrientation::kPortraitPrimary;
  is_disabled_ = false;
  receivers_.Clear();
}

bool FakeScreenOrientationImpl::UpdateDeviceOrientation(
    WebView* web_view,
    display::mojom::ScreenOrientation orientation) {
  Observe(web_view);

  if (device_orientation_ == orientation)
    return false;
  device_orientation_ = orientation;
  if (!IsOrientationAllowedByCurrentLock(orientation))
    return false;
  return UpdateScreenOrientation(orientation);
}

bool FakeScreenOrientationImpl::UpdateScreenOrientation(
    display::mojom::ScreenOrientation orientation) {
  if (current_orientation_ == orientation)
    return false;
  current_orientation_ = orientation;
  if (WebView* web_view = GetWebView()) {
    web_view->SetScreenOrientationOverrideForTesting(CurrentOrientationType());
    return true;
  }
  return false;
}

std::optional<display::mojom::ScreenOrientation>
FakeScreenOrientationImpl::CurrentOrientationType() const {
  if (is_disabled_)
    return std::nullopt;
  return current_orientation_;
}

void FakeScreenOrientationImpl::SetDisabled(WebView* web_view, bool disabled) {
  if (is_disabled_ == disabled)
    return;
  is_disabled_ = disabled;
  Observe(web_view);
  if (web_view) {
    web_view->SetScreenOrientationOverrideForTesting(CurrentOrientationType());
  }
}

bool FakeScreenOrientationImpl::IsOrientationAllowedByCurrentLock(
    display::mojom::ScreenOrientation orientation) {
  if (current_lock_ == device::mojom::ScreenOrientationLockType::DEFAULT ||
      current_lock_ == device::mojom::ScreenOrientationLockType::ANY) {
    return true;
  }

  switch (orientation) {
    case display::mojom::ScreenOrientation::kPortraitPrimary:
      return current_lock_ ==
                 device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY ||
             current_lock_ ==
                 device::mojom::ScreenOrientationLockType::PORTRAIT;
    case display::mojom::ScreenOrientation::kPortraitSecondary:
      return current_lock_ ==
                 device::mojom::ScreenOrientationLockType::PORTRAIT_SECONDARY ||
             current_lock_ ==
                 device::mojom::ScreenOrientationLockType::PORTRAIT;
    case display::mojom::ScreenOrientation::kLandscapePrimary:
      return current_lock_ ==
                 device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY ||
             current_lock_ ==
                 device::mojom::ScreenOrientationLockType::LANDSCAPE;
    case display::mojom::ScreenOrientation::kLandscapeSecondary:
      return current_lock_ == device::mojom::ScreenOrientationLockType::
                                  LANDSCAPE_SECONDARY ||
             current_lock_ ==
                 device::mojom::ScreenOrientationLockType::LANDSCAPE;
    default:
      return false;
  }
}

void FakeScreenOrientationImpl::AddReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  receivers_.Add(
      this, mojo::PendingAssociatedReceiver<device::mojom::ScreenOrientation>(
                std::move(handle)));
}

void FakeScreenOrientationImpl::OverrideAssociatedInterfaceProviderForFrame(
    blink::WebLocalFrame* frame) {
  if (!frame)
    return;

  content::RenderFrame* render_frame =
      content::RenderFrame::FromWebFrame(frame);
  blink::AssociatedInterfaceProvider* provider =
      render_frame->GetRemoteAssociatedInterfaces();

  provider->OverrideBinderForTesting(
      device::mojom::ScreenOrientation::Name_,
      base::BindRepeating(&FakeScreenOrientationImpl::AddReceiver,
                          base::Unretained(this)));
}

void FakeScreenOrientationImpl::LockOrientation(
    device::mojom::ScreenOrientationLockType orientation,
    LockOrientationCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeScreenOrientationImpl::UpdateLockSync,
                     base::Unretained(this), orientation, std::move(callback)));
}

void FakeScreenOrientationImpl::UnlockOrientation() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeScreenOrientationImpl::ResetLockSync,
                                base::Unretained(this)));
}

void FakeScreenOrientationImpl::UpdateLockSync(
    device::mojom::ScreenOrientationLockType lock,
    LockOrientationCallback callback) {
  DCHECK(lock != device::mojom::ScreenOrientationLockType::DEFAULT);
  current_lock_ = lock;
  if (!IsOrientationAllowedByCurrentLock(current_orientation_))
    UpdateScreenOrientation(SuitableOrientationForCurrentLock());
  std::move(callback).Run(device::mojom::ScreenOrientationLockResult::
                              SCREEN_ORIENTATION_LOCK_RESULT_SUCCESS);
}

void FakeScreenOrientationImpl::ResetLockSync() {
  bool will_screen_orientation_need_updating =
      !IsOrientationAllowedByCurrentLock(device_orientation_);
  current_lock_ = device::mojom::ScreenOrientationLockType::DEFAULT;
  if (will_screen_orientation_need_updating)
    UpdateScreenOrientation(device_orientation_);
}

display::mojom::ScreenOrientation
FakeScreenOrientationImpl::SuitableOrientationForCurrentLock() {
  switch (current_lock_) {
    case device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY:
      return display::mojom::ScreenOrientation::kPortraitSecondary;
    case device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY:
    case device::mojom::ScreenOrientationLockType::LANDSCAPE:
      return display::mojom::ScreenOrientation::kLandscapePrimary;
    case device::mojom::ScreenOrientationLockType::LANDSCAPE_SECONDARY:
      return display::mojom::ScreenOrientation::kLandscapePrimary;
    default:
      return display::mojom::ScreenOrientation::kPortraitPrimary;
  }
}

}  // namespace content
