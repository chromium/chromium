// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_voice_focused_web_state_tracker.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_utils.h"
#import "ios/web/public/web_state.h"

TtcVoiceFocusedWebStateTracker::TtcVoiceFocusedWebStateTracker(
    BrowserList* browser_list) {
  if (!browser_list) {
    return;
  }
  BindToBrowser(browser_list_utils::GetMostActiveSceneBrowser(browser_list));
}

TtcVoiceFocusedWebStateTracker::~TtcVoiceFocusedWebStateTracker() {
  Disconnect();
}

void TtcVoiceFocusedWebStateTracker::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  web_state_list_observation_.Reset();
  bound_browser_ = nullptr;
}

void TtcVoiceFocusedWebStateTracker::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!observer) {
    return;
  }
  observers_.AddObserver(observer);
}

void TtcVoiceFocusedWebStateTracker::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!observer) {
    return;
  }
  observers_.RemoveObserver(observer);
}

web::WebState* TtcVoiceFocusedWebStateTracker::GetActiveWebState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  WebStateList* web_state_list =
      bound_browser_ ? bound_browser_->GetWebStateList() : nullptr;
  return web_state_list ? web_state_list->GetActiveWebState() : nullptr;
}

#pragma mark - WebStateListObserver

void TtcVoiceFocusedWebStateTracker::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status.active_web_state_change()) {
    NotifyVoiceFocusedWebStateChanged(status.new_active_web_state);
  }
}

void TtcVoiceFocusedWebStateTracker::WebStateListDestroyed(
    WebStateList* web_state_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindToBrowser(nullptr);
}

#pragma mark - Private

void TtcVoiceFocusedWebStateTracker::BindToBrowser(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (bound_browser_.get() == browser) {
    return;
  }
  web::WebState* previous_web_state = GetActiveWebState();
  web_state_list_observation_.Reset();
  bound_browser_ = browser ? browser->AsWeakPtr() : nullptr;
  if (WebStateList* web_state_list =
          bound_browser_ ? bound_browser_->GetWebStateList() : nullptr) {
    web_state_list_observation_.Observe(web_state_list);
  }
  web::WebState* current_web_state = GetActiveWebState();
  if (current_web_state != previous_web_state) {
    NotifyVoiceFocusedWebStateChanged(current_web_state);
  }
}

void TtcVoiceFocusedWebStateTracker::NotifyVoiceFocusedWebStateChanged(
    web::WebState* active_web_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Observer& observer : observers_) {
    observer.OnVoiceFocusedWebStateChanged(active_web_state);
  }
}
