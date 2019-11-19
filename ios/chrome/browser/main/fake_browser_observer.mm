// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/fake_browser_observer.h"

#include "base/logging.h"
#import "ios/chrome/browser/main/browser.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FakeBrowserObserver::FakeBrowserObserver(Browser* browser) {
  DCHECK(browser);
  browser->AddObserver(this);
}

FakeBrowserObserver::~FakeBrowserObserver() = default;

void FakeBrowserObserver::BrowserDestroyed(Browser* browser) {
  browser->RemoveObserver(this);
  browser_destroyed_ = true;
}
