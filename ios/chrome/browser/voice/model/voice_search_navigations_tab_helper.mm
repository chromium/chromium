// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/model/voice_search_navigations_tab_helper.h"

#import <memory>

#import "base/check_op.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

#pragma mark - VoiceSearchNavigations

VoiceSearchNavigationTabHelper::VoiceSearchNavigationTabHelper(
    web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

void VoiceSearchNavigationTabHelper::WillLoadVoiceSearchResult() {
  will_navigate_to_voice_search_result_ = true;
}

bool VoiceSearchNavigationTabHelper::IsExpectingVoiceSearch() const {
  return will_navigate_to_voice_search_result_;
}

void VoiceSearchNavigationTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* context) {
  DCHECK_EQ(web_state_, web_state);
  if (will_navigate_to_voice_search_result_ && context->HasCommitted()) {
    will_navigate_to_voice_search_result_ = false;
  }
}

void VoiceSearchNavigationTabHelper::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

WEB_STATE_USER_DATA_KEY_IMPL(VoiceSearchNavigationTabHelper)
