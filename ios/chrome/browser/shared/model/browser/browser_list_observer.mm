// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/browser_list_observer.h"

#import <ostream>

#import "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BrowserListObserver::~BrowserListObserver() {
  DCHECK(!IsInObserverList())
      << "BrowserListObserver needs to be removed from BrowserList observer "
         "list before their destruction.";
}
