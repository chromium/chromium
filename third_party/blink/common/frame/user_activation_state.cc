// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/user_activation_state.h"

#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-shared.h"

namespace blink {

// The expiry time should be long enough to allow network round trips even in a
// very slow connection (to support xhr-like calls with user activation), yet
// not too long to make an "unattended" page feel activated.
constexpr base::TimeDelta kActivationLifespan = base::TimeDelta::FromSeconds(5);

UserActivationState::UserActivationState()
    : notification_type_(mojom::UserActivationNotificationType::kNone) {}

void UserActivationState::Activate(
    mojom::UserActivationNotificationType notification_type) {
  has_been_active_ = true;
  notification_type_ = notification_type;
  ActivateTransientState();
}

void UserActivationState::Clear() {
  has_been_active_ = false;
  notification_type_ = mojom::UserActivationNotificationType::kNone;
  DeactivateTransientState();
}

bool UserActivationState::HasBeenActive() const {
  // TODO(mustaq): Usecount notification_type_ if returning true.
  return has_been_active_;
}

bool UserActivationState::IsActive() const {
  // TODO(mustaq): Usecount notification_type_ if returning true.
  return base::TimeTicks::Now() <= transient_state_expiry_time_;
}

bool UserActivationState::ConsumeIfActive() {
  if (!IsActive())
    return false;
  // TODO(mustaq): Usecount notification_type_.
  DeactivateTransientState();
  return true;
}

void UserActivationState::ActivateTransientState() {
  transient_state_expiry_time_ = base::TimeTicks::Now() + kActivationLifespan;
}

void UserActivationState::DeactivateTransientState() {
  transient_state_expiry_time_ = base::TimeTicks();
}

}  // namespace blink
