// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider_interface.h"

#import "base/check.h"
#import "base/logging.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider.h"

@implementation StubBrowserProviderInterface {
  BOOL _shutdown;
}

- (instancetype)initWithBrowser:(Browser*)browser
               incognitoBrowser:(Browser*)incognitoBrowser {
  if ((self = [super init])) {
    _mainBrowserProvider =
        [[StubBrowserProvider alloc] initWithBrowser:browser];
    _incognitoBrowserProvider =
        [[StubBrowserProvider alloc] initWithBrowser:incognitoBrowser];
    _currentBrowserProvider = _mainBrowserProvider;
  }
  return self;
}

- (void)dealloc {
  CHECK(_shutdown) << "-shutdown must be called before -dealloc";
}

- (void)shutdown {
  _shutdown = YES;
  [_incognitoBrowserProvider shutdown];
  _incognitoBrowserProvider = nil;
  [_mainBrowserProvider shutdown];
  _mainBrowserProvider = nil;
}

#pragma mark - Properties

- (void)setCurrentBrowserProvider:(StubBrowserProvider*)currentBrowserProvider {
  CHECK(currentBrowserProvider == nil ||
        currentBrowserProvider == _mainBrowserProvider ||
        currentBrowserProvider == _incognitoBrowserProvider);
  _currentBrowserProvider = currentBrowserProvider;
}

#pragma mark - BrowserProviderInterface

- (BOOL)hasIncognitoBrowserProvider {
  return _incognitoBrowserProvider;
}

@end
