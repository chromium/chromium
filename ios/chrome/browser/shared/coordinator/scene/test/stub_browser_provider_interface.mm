// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider_interface.h"

#import "base/check.h"
#import "base/logging.h"
#import "base/memory/raw_ptr.h"

@implementation StubBrowserProviderInterface {
  raw_ptr<Browser> _currentBrowser;
  raw_ptr<Browser> _mainBrowser;
  raw_ptr<Browser> _incognitoBrowser;
  BOOL _shutdown;
}

- (instancetype)initWithBrowser:(Browser*)browser
               incognitoBrowser:(Browser*)incognitoBrowser {
  if ((self = [super init])) {
    CHECK(browser);
    CHECK(incognitoBrowser);
    _mainBrowser = browser;
    _incognitoBrowser = incognitoBrowser;
    _currentBrowser = _mainBrowser;
  }
  return self;
}

- (void)dealloc {
  CHECK(_shutdown) << "-shutdown must be called before -dealloc";
}

- (void)shutdown {
  _shutdown = YES;
  _currentBrowser = nullptr;
  _incognitoBrowser = nullptr;
  _mainBrowser = nullptr;
}

#pragma mark - Properties

- (void)setCurrentBrowser:(Browser*)currentBrowser {
  CHECK(currentBrowser == nullptr || currentBrowser == _mainBrowser.get() ||
        currentBrowser == _incognitoBrowser.get());
  _currentBrowser = currentBrowser;
}

- (void)setMainBrowser:(Browser*)mainBrowser {
  _mainBrowser = mainBrowser;
}

- (void)setIncognitoBrowser:(Browser*)incognitoBrowser {
  _incognitoBrowser = incognitoBrowser;
}

#pragma mark - BrowserProviderInterface

- (Browser*)currentBrowser {
  return _currentBrowser.get();
}

- (Browser*)mainBrowser {
  return _mainBrowser.get();
}

- (Browser*)incognitoBrowser {
  return _incognitoBrowser.get();
}

- (BOOL)hasIncognitoBrowser {
  return _incognitoBrowser != nullptr;
}

@end
