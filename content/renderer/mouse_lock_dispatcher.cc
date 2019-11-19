// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mouse_lock_dispatcher.h"

#include "base/logging.h"
#include "third_party/blink/public/platform/web_input_event.h"

namespace content {

MouseLockDispatcher::MouseLockDispatcher()
    : mouse_locked_(false),
      pending_lock_request_(false),
      pending_unlock_request_(false),
      target_(nullptr) {}

MouseLockDispatcher::~MouseLockDispatcher() {
}

bool MouseLockDispatcher::LockMouse(LockTarget* target,
                                    blink::WebLocalFrame* requester_frame,
                                    bool request_unadjusted_movement) {
  if (MouseLockedOrPendingAction())
    return false;

  pending_lock_request_ = true;
  target_ = target;

  SendLockMouseRequest(requester_frame, request_unadjusted_movement);
  return true;
}

void MouseLockDispatcher::UnlockMouse(LockTarget* target) {
  if (target && target == target_ && !pending_unlock_request_) {
    pending_unlock_request_ = true;

    SendUnlockMouseRequest();
  }
}

void MouseLockDispatcher::OnLockTargetDestroyed(LockTarget* target) {
  if (target == target_) {
    UnlockMouse(target);
    target_ = nullptr;
  }
}

void MouseLockDispatcher::ClearLockTarget() {
  OnLockTargetDestroyed(target_);
}

bool MouseLockDispatcher::IsMouseLockedTo(LockTarget* target) {
  return mouse_locked_ && target_ == target;
}

bool MouseLockDispatcher::WillHandleMouseEvent(
    const blink::WebMouseEvent& event) {
  if (mouse_locked_ && target_)
    return target_->HandleMouseLockedInputEvent(event);
  return false;
}

void MouseLockDispatcher::OnLockMouseACK(bool succeeded) {
  DCHECK(!mouse_locked_ && pending_lock_request_);

  mouse_locked_ = succeeded;
  pending_lock_request_ = false;
  if (pending_unlock_request_ && !succeeded) {
    // We have sent an unlock request after the lock request. However, since
    // the lock request has failed, the unlock request will be ignored by the
    // browser side and there won't be any response to it.
    pending_unlock_request_ = false;
  }

  LockTarget* last_target = target_;
  if (!succeeded)
    target_ = nullptr;

  // Callbacks made after all state modification to prevent reentrant errors
  // such as OnLockMouseACK() synchronously calling LockMouse().

  if (last_target)
    last_target->OnLockMouseACK(succeeded);
}

void MouseLockDispatcher::OnMouseLockLost() {
  DCHECK(mouse_locked_ && !pending_lock_request_);

  mouse_locked_ = false;
  pending_unlock_request_ = false;

  LockTarget* last_target = target_;
  target_ = nullptr;

  // Callbacks made after all state modification to prevent reentrant errors
  // such as OnMouseLockLost() synchronously calling LockMouse().

  if (last_target)
    last_target->OnMouseLockLost();
}

}  // namespace content
