// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser_state/browser_state_info_cache_observer.h"

#import <ostream>

#import "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BrowserStateInfoCacheObserver::~BrowserStateInfoCacheObserver() {
  CHECK(!IsInObserverList()) << "BrowserStateInfoCacheObserver needs to be "
                                "removed from BrowserStateInfoCache observer "
                                "list before their destruction.";
}
