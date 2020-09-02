// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/fake_screen_orientation_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/check.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/renderer/render_frame.h"
#include "content/web_test/renderer/web_view_test_proxy.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

FakeScreenOrientationImpl::FakeScreenOrientationImpl() = default;

FakeScreenOrientationImpl::~FakeScreenOrientationImpl() = default;

void FakeScreenOrientationImpl::ResetData() {
  web_view_test_proxy_ = nullptr;
  current_lock_ = device::mojom::ScreenOrientationLockType::DEFAULT;
  device_orientation_ = blink::mojom::ScreenOrientation::kPortraitPrimary;
  current_orientation_ = blink::mojom::ScreenOrientation::kPortraitPrimary;
  is_disabled_ = false;
  receivers_.Clear();
}

bool FakeScreenOrientationImpl::UpdateDeviceOrientation(
    WebViewTestProxy* web_view_test_proxy,
    blink::mojom::ScreenOrientation orientation) {
  web_view_test_proxy_ = web_view_test_proxy;

  if (device_orientation_ == orientation)
    return false;
  device_orientation_ = orientation;
  if (!IsOrientationAllowedByCurrentLock(orientation))
    return false;
  return UpdateScreenOrientation(orientation);
}

bool FakeScreenOrientationImpl::UpdateScreenOrientation(
    blink::mojom::ScreenOrientation orientation) {
  if (current_orientation_ == orientation)
    return false;
  current_orientation_ = orientation;
  if (web_view_test_proxy_) {
    web_view_test_proxy_->GetWebView()->SetScreenOrientationOverrideForTesting(
        CurrentOrientationType());
    return true;
  }
  return false;
}

base::Optional<blink::mojom::ScreenOrientation>
FakeScreenOrientationImpl::CurrentOrientationType() const {
  if (is_disabled_)
    return base::nullopt;
  return current_orientation_;
}

void FakeScreenOrientationImpl::SetDisabled(
    WebViewTestProxy* web_view_test_proxy,
    bool disabled) {
  if (is_disabled_ == disabled)
    return;
  is_disabled_ = disabled;
  web_view_test_proxy_ = web_view_test_proxy;
  if (web_view_test_proxy_) {
    web_view_test_proxy_->GetWebView()->SetScreenOrientationOverrideForTesting(
        CurrentOrientationType());
  }
}

bool FakeScreenOrientationImpl::IsOrientationAllowedByCurrentLock(
    blink::mojom::ScreenOrientation orientation) {
  if (current_lock_ == device::mojom::ScreenOrientationLockType::DEFAULT ||
      current_lock_ == device::mojom::ScreenOrientationLockType::ANY) {
    return true;
  }

  switch (orientation) {
    case blink::mojom::ScreenOrientation::kPortraitPrimary:
      return current_lock_ ==
                 device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY ||
             current_lock_ ==
                 device::mojom::ScreenOrientationLockType::PORTRAIT;
    case blink::mojom::ScreenOrientation::kPortraitSecondary:
      return current_lock_ ==
                 device::mojom::ScreenOrientationLockType::PORTRAIT_SECONDARY ||
             current_lock_ ==
                 device::mojom::ScreenOrientationLockType::PORTRAIT;
    case blink::mojom::ScreenOrientation::kLandscapePrimary:
      return current_lock_ ==
                 device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY ||
             current_lock_ ==
                 device::mojom::ScreenOrientationLockType::LANDSCAPE;
    case blink::mojom::ScreenOrientation::kLandscapeSecondary:
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeScreenOrientationImpl::UpdateLockSync,
                     base::Unretained(this), orientation, std::move(callback)));
}

void FakeScreenOrientationImpl::UnlockOrientation() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
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

blink::mojom::ScreenOrientation
FakeScreenOrientationImpl::SuitableOrientationForCurrentLock() {
  switch (current_lock_) {
    case device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY:
      return blink::mojom::ScreenOrientation::kPortraitSecondary;
    case device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY:
    case device::mojom::ScreenOrientationLockType::LANDSCAPE:
      return blink::mojom::ScreenOrientation::kLandscapePrimary;
    case device::mojom::ScreenOrientationLockType::LANDSCAPE_SECONDARY:
      return blink::mojom::ScreenOrientation::kLandscapePrimary;
    default:
      return blink::mojom::ScreenOrientation::kPortraitPrimary;
  }
}

}  // namespace content
