// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/test/fake_browser_observer.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

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
