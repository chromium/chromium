// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_browser_agent.h"

#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

namespace contextual_cueing {

ContextualCueingBrowserAgent::ContextualCueingBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {
  browser_observation_.Observe(browser_);
  web_state_list_observation_.Observe(browser_->GetWebStateList());
  // `GeminiBrowserAgent` is attached before this agent in
  // `AttachBrowserAgents()`, but may be absent for browsers where Gemini is
  // unavailable; in that case cues are never suppressed.
  if (GeminiBrowserAgent* gemini_browser_agent =
          GeminiBrowserAgent::FromBrowser(browser_)) {
    gemini_browser_agent_observation_.Observe(gemini_browser_agent);
    is_gemini_invoked_ = gemini_browser_agent->is_floaty_invoked();
  }
  TrackActiveWebState(browser_->GetWebStateList()->GetActiveWebState());
}

ContextualCueingBrowserAgent::~ContextualCueingBrowserAgent() = default;

#pragma mark - BrowserObserver

void ContextualCueingBrowserAgent::BrowserDestroyed(Browser* browser) {
  gemini_browser_agent_observation_.Reset();
  active_web_state_observation_.Reset();
  web_state_list_observation_.Reset();
  browser_observation_.Reset();
}

#pragma mark - GeminiBrowserAgent::Observer

void ContextualCueingBrowserAgent::OnFloatyInvokedChanged(bool is_invoked) {
  is_gemini_invoked_ = is_invoked;
  // Gemini is invoked per window, and only the active tab can show a cue, so
  // background tabs are brought up to date when they become active instead.
  TrackActiveWebState(browser_->GetWebStateList()->GetActiveWebState());
}

#pragma mark - WebStateListObserver

void ContextualCueingBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  if (!status.active_web_state_change()) {
    return;
  }
  TrackActiveWebState(status.new_active_web_state);
}

#pragma mark - web::WebStateObserver

void ContextualCueingBrowserAgent::WebStateRealized(web::WebState* web_state) {
  // `BrowserWebStateListDelegate` observes the web state since its insertion,
  // i.e. before this agent, so its tab helpers are already attached here.
  active_web_state_observation_.Reset();
  UpdateGeminiInvokedForWebState(web_state);
}

void ContextualCueingBrowserAgent::WebStateDestroyed(web::WebState* web_state) {
  active_web_state_observation_.Reset();
}

#pragma mark - Private

void ContextualCueingBrowserAgent::TrackActiveWebState(
    web::WebState* web_state) {
  active_web_state_observation_.Reset();
  if (!web_state) {
    return;
  }
  // Avoid forcing realization: unrealized web states have no tab helper yet,
  // so wait for `WebStateRealized()` instead.
  if (!web_state->IsRealized()) {
    active_web_state_observation_.Observe(web_state);
    return;
  }
  UpdateGeminiInvokedForWebState(web_state);
}

void ContextualCueingBrowserAgent::UpdateGeminiInvokedForWebState(
    web::WebState* web_state) {
  if (!web_state || !web_state->IsRealized()) {
    return;
  }
  ContextualCueingTabHelper* tab_helper =
      ContextualCueingTabHelper::FromWebState(web_state);
  if (!tab_helper) {
    return;
  }
  tab_helper->SetGeminiInvoked(is_gemini_invoked_);
}

}  // namespace contextual_cueing
