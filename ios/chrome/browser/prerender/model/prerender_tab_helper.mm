// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/model/prerender_tab_helper.h"

#import "base/check.h"
#import "base/check_deref.h"
#import "ios/chrome/browser/prerender/model/prerender_tab_helper_delegate.h"

PrerenderTabHelper::PrerenderTabHelper(web::WebState* web_state,
                                       PrerenderTabHelperDelegate* delegate)
    : delegate_(CHECK_DEREF(delegate)) {}

PrerenderTabHelper::~PrerenderTabHelper() = default;

void PrerenderTabHelper::CancelPrerender() {
  delegate_->CancelPrerender();
}
