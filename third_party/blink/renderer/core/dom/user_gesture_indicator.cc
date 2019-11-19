// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"

#include "base/time/default_clock.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

// User gestures timeout in 1 second.
const double kUserGestureTimeout = 1.0;

UserGestureToken::UserGestureToken()
    : consumable_gestures_(0),
      clock_(base::DefaultClock::GetInstance()),
      timestamp_(clock_->Now().ToDoubleT()) {
  consumable_gestures_++;
}

bool UserGestureToken::HasGestures() const {
  return consumable_gestures_ && !HasTimedOut();
}

void UserGestureToken::TransferGestureTo(UserGestureToken* other) {
  if (!HasGestures())
    return;
  consumable_gestures_--;
  other->consumable_gestures_++;
}

bool UserGestureToken::ConsumeGesture() {
  if (!consumable_gestures_)
    return false;
  consumable_gestures_--;
  return true;
}

void UserGestureToken::ResetTimestamp() {
  timestamp_ = clock_->Now().ToDoubleT();
}

bool UserGestureToken::HasTimedOut() const {
  return clock_->Now().ToDoubleT() - timestamp_ > kUserGestureTimeout;
}

UserGestureToken* UserGestureIndicator::root_token_ = nullptr;

void UserGestureIndicator::UpdateRootToken() {
  if (!root_token_)
    root_token_ = token_.get();
  else
    token_->TransferGestureTo(root_token_);
}

UserGestureIndicator::UserGestureIndicator(
    scoped_refptr<UserGestureToken> token) {
  if (!IsMainThread() || !token || token == root_token_)
    return;
  token_ = std::move(token);
  token_->ResetTimestamp();
  UpdateRootToken();
}

UserGestureIndicator::UserGestureIndicator() {
  if (!IsMainThread())
    return;
  token_ = base::AdoptRef(new UserGestureToken());
  UpdateRootToken();
}

UserGestureIndicator::~UserGestureIndicator() {
  if (IsMainThread() && token_ && token_ == root_token_)
    root_token_ = nullptr;
}

// static
UserGestureToken* UserGestureIndicator::CurrentTokenForTest() {
  DCHECK(IsMainThread());
  return root_token_;
}

// static
UserGestureToken* UserGestureIndicator::CurrentTokenThreadSafe() {
  return IsMainThread() ? root_token_ : nullptr;
}

}  // namespace blink
