// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/mock_screen_orientation_client.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace test_runner {

MockScreenOrientationClient::MockScreenOrientationClient()
    : main_frame_(nullptr),
      current_lock_(blink::kWebScreenOrientationLockDefault),
      device_orientation_(blink::kWebScreenOrientationPortraitPrimary),
      current_orientation_(blink::kWebScreenOrientationPortraitPrimary),
      is_disabled_(false) {}

MockScreenOrientationClient::~MockScreenOrientationClient() {}

void MockScreenOrientationClient::ResetData() {
  main_frame_ = nullptr;
  current_lock_ = blink::kWebScreenOrientationLockDefault;
  device_orientation_ = blink::kWebScreenOrientationPortraitPrimary;
  current_orientation_ = blink::kWebScreenOrientationPortraitPrimary;
  is_disabled_ = false;
  receivers_.Clear();
}

void MockScreenOrientationClient::UpdateDeviceOrientation(
    blink::WebLocalFrame* main_frame,
    blink::WebScreenOrientationType orientation) {
  main_frame_ = main_frame;

  if (device_orientation_ == orientation)
    return;
  device_orientation_ = orientation;
  if (!IsOrientationAllowedByCurrentLock(orientation))
    return;
  UpdateScreenOrientation(orientation);
}

void MockScreenOrientationClient::UpdateScreenOrientation(
    blink::WebScreenOrientationType orientation) {
  if (current_orientation_ == orientation)
    return;
  current_orientation_ = orientation;
  if (main_frame_)
    main_frame_->SendOrientationChangeEvent();
}

blink::WebScreenOrientationType
MockScreenOrientationClient::CurrentOrientationType() const {
  return current_orientation_;
}

unsigned MockScreenOrientationClient::CurrentOrientationAngle() const {
  return OrientationTypeToAngle(current_orientation_);
}

void MockScreenOrientationClient::SetDisabled(bool disabled) {
  is_disabled_ = disabled;
}

unsigned MockScreenOrientationClient::OrientationTypeToAngle(
    blink::WebScreenOrientationType type) {
  unsigned angle;
  // FIXME(ostap): This relationship between orientationType and
  // orientationAngle is temporary. The test should be able to specify
  // the angle in addition to the orientation type.
  switch (type) {
    case blink::kWebScreenOrientationLandscapePrimary:
      angle = 90;
      break;
    case blink::kWebScreenOrientationLandscapeSecondary:
      angle = 270;
      break;
    case blink::kWebScreenOrientationPortraitSecondary:
      angle = 180;
      break;
    default:
      angle = 0;
  }
  return angle;
}

bool MockScreenOrientationClient::IsOrientationAllowedByCurrentLock(
    blink::WebScreenOrientationType orientation) {
  if (current_lock_ == blink::kWebScreenOrientationLockDefault ||
      current_lock_ == blink::kWebScreenOrientationLockAny) {
    return true;
  }

  switch (orientation) {
    case blink::kWebScreenOrientationPortraitPrimary:
      return current_lock_ == blink::kWebScreenOrientationLockPortraitPrimary ||
             current_lock_ == blink::kWebScreenOrientationLockPortrait;
    case blink::kWebScreenOrientationPortraitSecondary:
      return current_lock_ ==
                 blink::kWebScreenOrientationLockPortraitSecondary ||
             current_lock_ == blink::kWebScreenOrientationLockPortrait;
    case blink::kWebScreenOrientationLandscapePrimary:
      return current_lock_ ==
                 blink::kWebScreenOrientationLockLandscapePrimary ||
             current_lock_ == blink::kWebScreenOrientationLockLandscape;
    case blink::kWebScreenOrientationLandscapeSecondary:
      return current_lock_ ==
                 blink::kWebScreenOrientationLockLandscapeSecondary ||
             current_lock_ == blink::kWebScreenOrientationLockLandscape;
    default:
      return false;
  }
}

void MockScreenOrientationClient::AddReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  receivers_.Add(
      this, mojo::PendingAssociatedReceiver<device::mojom::ScreenOrientation>(
                std::move(handle)));
}

void MockScreenOrientationClient::OverrideAssociatedInterfaceProviderForFrame(
    blink::WebLocalFrame* frame) {
  if (!frame)
    return;

  content::RenderFrame* render_frame =
      content::RenderFrame::FromWebFrame(frame);
  blink::AssociatedInterfaceProvider* provider =
      render_frame->GetRemoteAssociatedInterfaces();

  provider->OverrideBinderForTesting(
      device::mojom::ScreenOrientation::Name_,
      base::BindRepeating(&MockScreenOrientationClient::AddReceiver,
                          base::Unretained(this)));
}

void MockScreenOrientationClient::LockOrientation(
    blink::WebScreenOrientationLockType orientation,
    LockOrientationCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockScreenOrientationClient::UpdateLockSync,
                     base::Unretained(this), orientation, std::move(callback)));
}

void MockScreenOrientationClient::UnlockOrientation() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MockScreenOrientationClient::ResetLockSync,
                                base::Unretained(this)));
}

void MockScreenOrientationClient::UpdateLockSync(
    blink::WebScreenOrientationLockType lock,
    LockOrientationCallback callback) {
  DCHECK(lock != blink::kWebScreenOrientationLockDefault);
  current_lock_ = lock;
  if (!IsOrientationAllowedByCurrentLock(current_orientation_))
    UpdateScreenOrientation(SuitableOrientationForCurrentLock());
  std::move(callback).Run(device::mojom::ScreenOrientationLockResult::
                              SCREEN_ORIENTATION_LOCK_RESULT_SUCCESS);
}

void MockScreenOrientationClient::ResetLockSync() {
  bool will_screen_orientation_need_updating =
      !IsOrientationAllowedByCurrentLock(device_orientation_);
  current_lock_ = blink::kWebScreenOrientationLockDefault;
  if (will_screen_orientation_need_updating)
    UpdateScreenOrientation(device_orientation_);
}

blink::WebScreenOrientationType
MockScreenOrientationClient::SuitableOrientationForCurrentLock() {
  switch (current_lock_) {
    case blink::kWebScreenOrientationLockPortraitSecondary:
      return blink::kWebScreenOrientationPortraitSecondary;
    case blink::kWebScreenOrientationLockLandscapePrimary:
    case blink::kWebScreenOrientationLockLandscape:
      return blink::kWebScreenOrientationLandscapePrimary;
    case blink::kWebScreenOrientationLockLandscapeSecondary:
      return blink::kWebScreenOrientationLandscapePrimary;
    default:
      return blink::kWebScreenOrientationPortraitPrimary;
  }
}

}  // namespace test_runner
