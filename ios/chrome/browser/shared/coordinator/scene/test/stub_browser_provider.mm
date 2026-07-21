// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider.h"

#import "base/check.h"
#import "base/logging.h"
#import "base/memory/raw_ptr.h"

@implementation StubBrowserProvider {
  raw_ptr<Browser> _browser;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  if ((self = [super init])) {
    CHECK(browser);
    _browser = browser;
  }
  return self;
}

- (void)dealloc {
  CHECK(!_browser) << "-shutdown must be called before -dealloc";
}

- (void)shutdown {
  _browser = nullptr;
}

- (Browser*)browser {
  return _browser.get();
}

- (void)setBrowser:(Browser*)browser {
  _browser = browser;
}

- (UIViewController*)viewController:(BrowserProviderPassKey)key {
  return nil;
}

@end
