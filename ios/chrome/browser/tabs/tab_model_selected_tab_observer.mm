// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_model_selected_tab_observer.h"

#include "base/logging.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabModelSelectedTabObserver {
  __weak TabModel* _tabModel;
}

- (instancetype)initWithTabModel:(TabModel*)tabModel {
  DCHECK(tabModel);
  if ((self = [super init])) {
    _tabModel = tabModel;
  }
  return self;
}

#pragma mark WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(int)reason {
  if (oldWebState) {
    // Save state, such as scroll position, ... of the old selected Tab.
    Tab* oldTab = LegacyTabHelper::GetTabForWebState(oldWebState);
    DCHECK(oldTab);

    // Avoid artificially extending the lifetime of oldTab until the global
    // autoreleasepool is purged.
    @autoreleasepool {
      [_tabModel notifyTabWasDeselected:oldTab];
    }
  }

  if (newWebState) {
    Tab* newTab = LegacyTabHelper::GetTabForWebState(newWebState);

    // Persist the session state.
    if (newTab.loadFinished)
      [_tabModel saveSessionImmediately:NO];
  }
}

@end
