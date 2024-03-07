// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/user_activation_state.h"

#include "base/metrics/histogram_functions.h"
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

UserActivationState::UserActivationState()
    : first_notification_type_(UserActivationNotificationType::kNone),
      last_notification_type_(UserActivationNotificationType::kNone) {}

void UserActivationState::Activate(
    UserActivationNotificationType notification_type) {
  has_been_active_ = true;
  last_activation_was_restricted_ = IsRestricted(notification_type);
  ActivateTransientState();

  // Update states for UMA.
  DCHECK(notification_type != UserActivationNotificationType::kNone);
  if (first_notification_type_ == UserActivationNotificationType::kNone)
    first_notification_type_ = notification_type;
  last_notification_type_ = notification_type;
  if (notification_type == UserActivationNotificationType::kInteraction)
    transient_state_expiry_time_for_interaction_ = transient_state_expiry_time_;
}

void UserActivationState::SetHasBeenActive() {
  has_been_active_ = true;
}

void UserActivationState::Clear() {
  has_been_active_ = false;
  last_activation_was_restricted_ = false;
  first_notification_type_ = UserActivationNotificationType::kNone;
  last_notification_type_ = UserActivationNotificationType::kNone;
  DeactivateTransientState();
}

bool UserActivationState::HasBeenActive() const {
  if (has_been_active_) {
    base::UmaHistogramEnumeration("Event.UserActivation.TriggerForSticky",
                                  first_notification_type_);
    return true;
  }
  return false;
}

bool UserActivationState::IsActive() const {
  if (IsActiveInternal()) {
    base::UmaHistogramEnumeration("Event.UserActivation.TriggerForTransient",
                                  EffectiveNotificationType());
    return true;
  }
  return false;
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

void UserActivationState::RecordPreconsumptionUma() const {
  if (!IsActiveInternal())
    return;
  base::UmaHistogramEnumeration("Event.UserActivation.TriggerForConsuming",
                                EffectiveNotificationType());
}

void UserActivationState::ActivateTransientState() {
  transient_state_expiry_time_ = base::TimeTicks::Now() + kActivationLifespan;
}

void UserActivationState::DeactivateTransientState() {
  transient_state_expiry_time_ = base::TimeTicks();
  transient_state_expiry_time_for_interaction_ = transient_state_expiry_time_;
}

UserActivationNotificationType UserActivationState::EffectiveNotificationType()
    const {
  // We treat a synthetic activation within the expiry time of a real
  // interaction (of type kInteraction) as a real interaction because any user
  // of transient activation state should work within that expiry time even if
  // we drop all synthetic activations.
  return base::TimeTicks::Now() <= transient_state_expiry_time_for_interaction_
             ? UserActivationNotificationType::kInteraction
             : last_notification_type_;
}

}  // namespace blink
