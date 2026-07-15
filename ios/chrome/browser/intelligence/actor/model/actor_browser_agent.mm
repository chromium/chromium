// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_browser_agent.h"

#import "base/check.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"
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

void ActorBrowserAgent::OnActuationStateChanged(ActorTabHelper* tab_helper,
                                                web::WebState* web_state,
                                                bool actuating) {
  id<ActorOverlayCommands> handler = HandlerForProtocol(
      browser_->GetCommandDispatcher(), ActorOverlayCommands);
  if (actuating) {
    [handler showActorOverlayForWebState:web_state];
  } else {
    [handler hideActorOverlay];
  }
}

void ActorBrowserAgent::UpdateActiveWebState(web::WebState* old_web_state,
                                             web::WebState* new_web_state) {
  if (old_web_state == new_web_state) {
    return;
  }

  tab_helper_observation_.Reset();

  ActorTabHelper* old_tab_helper =
      old_web_state ? ActorTabHelper::FromWebState(old_web_state) : nullptr;
  const bool old_state_was_actuating =
      old_tab_helper && old_tab_helper->IsActuating();

  ActorTabHelper* new_tab_helper =
      new_web_state ? ActorTabHelper::FromWebState(new_web_state) : nullptr;
  const bool new_state_is_actuating =
      new_tab_helper && new_tab_helper->IsActuating();

  if (old_state_was_actuating || new_state_is_actuating) {
    id<ActorOverlayCommands> handler = HandlerForProtocol(
        browser_->GetCommandDispatcher(), ActorOverlayCommands);
    if (old_state_was_actuating) {
      [handler hideActorOverlay];
    }
    if (new_state_is_actuating) {
      [handler showActorOverlayForWebState:new_web_state];
    }
  }

  if (new_tab_helper) {
    tab_helper_observation_.Observe(new_tab_helper);
  }
}
