// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/follow_tab_helper.h"

#include "base/memory/ptr_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FollowTabHelper::~FollowTabHelper() {
  DCHECK(!web_state_);
}

// static
void FollowTabHelper::CreateForWebState(web::WebState* web_state) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(),
                           base::WrapUnique(new FollowTabHelper(web_state)));
  }
}

FollowTabHelper::FollowTabHelper(web::WebState* web_state)
    : web_state_(web_state), weak_ptr_factory_(this) {
  DCHECK(web_state_);
  web_state_observation_.Observe(web_state_);
}

void FollowTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  switch (load_completion_status) {
    case web::PageLoadCompletionStatus::FAILURE:
      break;
    case web::PageLoadCompletionStatus::SUCCESS:
      // TODO(crbug.com/1318755): Get recommended status from follow provider.
      // If recommended, use feature engagement tracker to decide whether to
      // show the while-browsing IPH.
      break;
  }
}

void FollowTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  DCHECK(web_state_observation_.IsObservingSource(web_state));
  web_state_observation_.Reset();
  web_state_ = nullptr;
}

WEB_STATE_USER_DATA_KEY_IMPL(FollowTabHelper)
