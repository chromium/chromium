// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper_observer.h"

#import "ios/chrome/browser/intelligence/actor/public/actor_control_state.h"

void ActorTabHelperObserver::OnControlStateChanged(
    ActorTabHelper* tab_helper,
    web::WebState* web_state,
    actor::ActorControlState previous_control_state,
    actor::ActorControlState new_control_state) {
  // TODO(crbug.com/548051839): Deprecated, remove once callers are updated.
  const bool previous_active =
      previous_control_state == actor::ActorControlState::kActorControlled;
  const bool new_active =
      new_control_state == actor::ActorControlState::kActorControlled;
  if (previous_active != new_active) {
    OnActuationStateChanged(tab_helper, web_state, new_active);
  }
}
