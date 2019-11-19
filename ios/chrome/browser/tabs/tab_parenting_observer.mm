// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_parenting_observer.h"

#include "ios/chrome/browser/tabs/tab_parenting_global_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TabParentingObserver::TabParentingObserver() = default;

TabParentingObserver::~TabParentingObserver() = default;

void TabParentingObserver::WebStateInsertedAt(WebStateList* web_state_list,
                                              web::WebState* web_state,
                                              int index,
                                              bool activating) {
  TabParentingGlobalObserver::GetInstance()->OnTabParented(web_state);
}

void TabParentingObserver::WebStateReplacedAt(WebStateList* web_state_list,
                                              web::WebState* old_web_state,
                                              web::WebState* new_web_state,
                                              int index) {
  TabParentingGlobalObserver::GetInstance()->OnTabParented(new_web_state);
}
