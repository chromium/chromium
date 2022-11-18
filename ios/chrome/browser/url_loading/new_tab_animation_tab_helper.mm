// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/new_tab_animation_tab_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WEB_STATE_USER_DATA_KEY_IMPL(NewTabAnimationTabHelper)

NewTabAnimationTabHelper::NewTabAnimationTabHelper(web::WebState* web_state)
    : animation_disabled_(false) {}

NewTabAnimationTabHelper::~NewTabAnimationTabHelper() {}

void NewTabAnimationTabHelper::DisableNewTabAnimation() {
  animation_disabled_ = true;
}

bool NewTabAnimationTabHelper::ShouldAnimateNewTab() const {
  return !animation_disabled_;
}
