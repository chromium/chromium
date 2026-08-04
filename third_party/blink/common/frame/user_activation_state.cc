// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/user_activation_state.h"

#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-shared.h"

using blink::mojom::UserActivationNotificationType;

namespace blink {

namespace {

// Indicates if |notification_type| should be considered restricted.  See
// |LastActivationWasRestricted| for details.
bool IsRestricted(UserActivationNotificationType notification_type) {
  return notification_type == UserActivationNotificationType::
                                  kExtensionMessagingBothPrivileged ||
         notification_type == UserActivationNotificationType::
                                  kExtensionMessagingSenderPrivileged ||
         notification_type == UserActivationNotificationType::
                                  kExtensionMessagingReceiverPrivileged ||
         notification_type == UserActivationNotificationType::
                                  kExtensionMessagingNeitherPrivileged;
}

}  // namespace


void UserActivationState::Activate(
    UserActivationNotificationType notification_type) {
  has_been_active_ = true;
  last_activation_was_restricted_ = IsRestricted(notification_type);
  ActivateTransientState();
}

void UserActivationState::SetHasBeenActive() {
  has_been_active_ = true;
}

void UserActivationState::Clear() {
  has_been_active_ = false;
  last_activation_was_restricted_ = false;
  DeactivateTransientState();
}

bool UserActivationState::HasBeenActive() const {
  return has_been_active_;
}

bool UserActivationState::IsActive() const {
  return IsActiveInternal();
}

bool UserActivationState::IsActiveInternal() const {
  return base::TimeTicks::Now() <= transient_state_expiry_time_;
}

bool UserActivationState::ConsumeIfActive() {
  if (!IsActiveInternal())
    return false;
  DeactivateTransientState();
  return true;
}

bool UserActivationState::LastActivationWasRestricted() const {
  return last_activation_was_restricted_;
}

void UserActivationState::ActivateTransientState() {
  transient_state_expiry_time_ = base::TimeTicks::Now() + kActivationLifespan;
}

void UserActivationState::DeactivateTransientState() {
  transient_state_expiry_time_ = base::TimeTicks();
}

}  // namespace blink
