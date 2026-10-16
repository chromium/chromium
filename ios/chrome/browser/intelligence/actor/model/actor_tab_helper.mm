// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"

#import "base/functional/bind.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_control_state.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"

ActorTabHelper::ActorTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_observation_.Observe(web_state);
}

ActorTabHelper::~ActorTabHelper() {
  // Clear suppression if the tab helper is torn down mid-actuation, so that a
  // surviving web view proxy is not left with input views permanently
  // suppressed. Messaging a `nil` proxy is a no-op.
  if (control_state_ == actor::ActorControlState::kActorControlled &&
      web_state_ && ActorDisablesInputDuringActuation()) {
    web_state_->GetWebViewProxy().shouldSuppressInputViews = NO;
  }
}

void ActorTabHelper::SetControlState(actor::ActorControlState control_state) {
  if (control_state_ == control_state) {
    return;
  }
  const actor::ActorControlState previous_control_state = control_state_;
  control_state_ = control_state;
  ApplyKeepRenderProcessAlive();

  const bool previous_active =
      previous_control_state == actor::ActorControlState::kActorControlled;
  const bool new_active =
      control_state == actor::ActorControlState::kActorControlled;
  if (previous_active != new_active && web_state_ &&
      ActorDisablesInputDuringActuation()) {
    // Suppress the software keyboard and input accessory views while actuating
    // to prevent viewport shifts and keyboard occlusion, and restore them when
    // actuation ends.
    web_state_->GetWebViewProxy().shouldSuppressInputViews = new_active;
  }

  control_state_callbacks_.Notify(previous_control_state, control_state);

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

#pragma mark - web::WebStateObserver

void ActorTabHelper::WasHidden(web::WebState* web_state) {
  if (control_state_ == actor::ActorControlState::kInactive) {
    return;
  }

  // The browser resets keep-alive synchronously right after `WasHidden()`, so
  // re-applying it must be deferred to a posted task.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ActorTabHelper::ApplyKeepRenderProcessAlive,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ActorTabHelper::WebStateDestroyed(web::WebState* web_state) {
  if (control_state_ == actor::ActorControlState::kActorControlled &&
      web_state_ && ActorDisablesInputDuringActuation()) {
    web_state_->GetWebViewProxy().shouldSuppressInputViews = NO;
  }
  web_state_observation_.Reset();
  web_state_ = nullptr;
}

#pragma mark - Private

void ActorTabHelper::ApplyKeepRenderProcessAlive() {
  if (!web_state_) {
    return;
  }
  web_state_->SetKeepRenderProcessAlive(control_state_ !=
                                        actor::ActorControlState::kInactive);
}
