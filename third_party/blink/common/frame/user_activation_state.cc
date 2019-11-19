// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/user_activation_state.h"

namespace blink {

// The expiry time should be long enough to allow network round trips even in a
// very slow connection (to support xhr-like calls with user activation), yet
// not too long to make an "unattneded" page feel activated.
constexpr base::TimeDelta kActivationLifespan = base::TimeDelta::FromSeconds(5);

void UserActivationState::Activate() {
  has_been_active_ = true;
  ActivateTransientState();
}

void UserActivationState::Clear() {
  has_been_active_ = false;
  DeactivateTransientState();
}

bool UserActivationState::IsActive() const {
  return base::TimeTicks::Now() <= transient_state_expiry_time_;
}

bool UserActivationState::ConsumeIfActive() {
  if (!IsActive())
    return false;
  DeactivateTransientState();
  return true;
}

void UserActivationState::TransferFrom(UserActivationState& other) {
  if (other.has_been_active_)
    has_been_active_ = true;
  if (transient_state_expiry_time_ < other.transient_state_expiry_time_)
    transient_state_expiry_time_ = other.transient_state_expiry_time_;

  other.Clear();
}

void UserActivationState::ActivateTransientState() {
  transient_state_expiry_time_ = base::TimeTicks::Now() + kActivationLifespan;
}

void UserActivationState::DeactivateTransientState() {
  transient_state_expiry_time_ = base::TimeTicks();
}

}  // namespace blink
