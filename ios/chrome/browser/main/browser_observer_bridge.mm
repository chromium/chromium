// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_observer_bridge.h"

#import <CoreFoundation/CoreFoundation.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BrowserObserverBridge::BrowserObserverBridge(Browser* browser,
                                             id<BrowserObserving> observer)
    : observer_(observer) {
  browser_observation_.Observe(browser);
}

BrowserObserverBridge::~BrowserObserverBridge() {}

void BrowserObserverBridge::BrowserDestroyed(Browser* browser) {
  DCHECK(browser_observation_.IsObservingSource(browser));
  browser_observation_.Reset();

  // The following invocation may lead to the destruction
  // of `this`. No code should follow.
  [observer_ browserDestroyed:browser];
}
