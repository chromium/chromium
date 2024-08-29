// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider_interface.h"

#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider.h"

@implementation StubBrowserProviderInterface

- (instancetype)init {
  if ((self = [super init])) {
    _mainBrowserProvider = [[StubBrowserProvider alloc] init];
    _incognitoBrowserProvider = [[StubBrowserProvider alloc] init];
    _currentBrowserProvider = _mainBrowserProvider;
  }
  return self;
}

#pragma mark - BrowserProviderInterface

- (BOOL)hasIncognitoBrowserProvider {
  return _incognitoBrowserProvider;
}

@end
