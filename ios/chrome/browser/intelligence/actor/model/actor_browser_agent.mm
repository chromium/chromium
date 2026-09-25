// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_browser_agent.h"

#import "base/check.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_control_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/actor_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/web/public/web_state.h"

ActorBrowserAgent::ActorBrowserAgent(Browser* browser)
    : BrowserUserData<ActorBrowserAgent>(browser),
      browser_id_(SessionID::NewUnique()) {
  CHECK(browser);
  web_state_list_observation_.Observe(browser->GetWebStateList());
  UpdateActiveWebState(nullptr,
                       browser->GetWebStateList()->GetActiveWebState());
}

ActorBrowserAgent::~ActorBrowserAgent() = default;

SessionID ActorBrowserAgent::browser_id() const {
  return browser_id_;
}

void ActorBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  if (status.active_web_state_change()) {
    UpdateActiveWebState(status.old_active_web_state,
                         status.new_active_web_state);
  }
}

void ActorBrowserAgent::WebStateListDestroyed(WebStateList* web_state_list) {
  tab_helper_observation_.Reset();
  web_state_list_observation_.Reset();
}

void ActorBrowserAgent::OnControlStateChanged(
    ActorTabHelper* tab_helper,
    web::WebState* web_state,
    actor::ActorControlState previous_control_state,
    actor::ActorControlState new_control_state) {
  UpdateOverlayForControlState(new_control_state, web_state);
}

void ActorBrowserAgent::UpdateActiveWebState(web::WebState* old_web_state,
                                             web::WebState* new_web_state) {
  if (old_web_state == new_web_state) {
    return;
  }

  tab_helper_observation_.Reset();

  ActorTabHelper* old_tab_helper =
      old_web_state ? ActorTabHelper::FromWebState(old_web_state) : nullptr;
  const actor::ActorControlState old_control_state =
      old_tab_helper ? old_tab_helper->GetControlState()
                     : actor::ActorControlState::kInactive;

  ActorTabHelper* new_tab_helper =
      new_web_state ? ActorTabHelper::FromWebState(new_web_state) : nullptr;
  const actor::ActorControlState new_control_state =
      new_tab_helper ? new_tab_helper->GetControlState()
                     : actor::ActorControlState::kInactive;

  if (old_control_state != new_control_state) {
    UpdateOverlayForControlState(new_control_state, new_web_state);
  }

  if (new_tab_helper) {
    tab_helper_observation_.Observe(new_tab_helper);
  }
}

void ActorBrowserAgent::UpdateOverlayForControlState(
    actor::ActorControlState control_state,
    web::WebState* web_state) {
  id<ActorOverlayCommands> handler = HandlerForProtocol(
      browser_->GetCommandDispatcher(), ActorOverlayCommands);
  switch (control_state) {
    case actor::ActorControlState::kInactive:
      [handler hideActorOverlay];
      break;
    case actor::ActorControlState::kActorControlled:
      [handler showActorOverlayForWebState:web_state];
      break;
  }
}
