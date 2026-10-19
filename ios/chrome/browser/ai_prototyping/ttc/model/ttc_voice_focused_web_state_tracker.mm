// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_voice_focused_web_state_tracker.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
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
  bound_browser_ = nullptr;
}

web::WebState* TtcVoiceFocusedWebStateTracker::GetActiveWebState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  WebStateList* web_state_list =
      bound_browser_ ? bound_browser_->GetWebStateList() : nullptr;
  return web_state_list ? web_state_list->GetActiveWebState() : nullptr;
}

#pragma mark - Private

void TtcVoiceFocusedWebStateTracker::BindToBrowser(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bound_browser_ = browser ? browser->AsWeakPtr() : nullptr;
}
