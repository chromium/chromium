// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"

#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_control_state.h"

ActorTabHelper::ActorTabHelper(web::WebState* web_state)
    : web_state_(web_state) {}

ActorTabHelper::~ActorTabHelper() = default;

void ActorTabHelper::SetControlState(actor::ActorControlState control_state) {
  if (control_state_ == control_state) {
    return;
  }
  const actor::ActorControlState previous_control_state = control_state_;
  control_state_ = control_state;
  control_state_callbacks_.Notify(previous_control_state, control_state);

  const bool previous_active =
      previous_control_state == actor::ActorControlState::kActorControlled;
  const bool new_active =
      control_state == actor::ActorControlState::kActorControlled;
  if (previous_active != new_active) {
    actuation_state_callbacks_.Notify(new_active);
  }

  for (ActorTabHelperObserver& observer : observers_) {
    observer.OnControlStateChanged(this, web_state_, previous_control_state,
                                   control_state);
  }
}

void ActorTabHelper::SetActuating(bool actuating) {
  SetControlState(actuating ? actor::ActorControlState::kActorControlled
                            : actor::ActorControlState::kInactive);
}

actor::ActorControlState ActorTabHelper::GetControlState() const {
  return control_state_;
}

bool ActorTabHelper::IsActuating() const {
  return control_state_ == actor::ActorControlState::kActorControlled;
}

base::CallbackListSubscription ActorTabHelper::AddControlStateChangedCallback(
    ControlStateCallback callback) {
  return control_state_callbacks_.Add(std::move(callback));
}

base::CallbackListSubscription ActorTabHelper::AddActuationStateChangedCallback(
    ActuationStateCallback callback) {
  return actuation_state_callbacks_.Add(std::move(callback));
}

void ActorTabHelper::AddObserver(ActorTabHelperObserver* observer) {
  observers_.AddObserver(observer);
}

void ActorTabHelper::RemoveObserver(ActorTabHelperObserver* observer) {
  observers_.RemoveObserver(observer);
}
