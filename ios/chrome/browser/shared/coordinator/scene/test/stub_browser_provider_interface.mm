// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider_interface.h"

#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation StubBrowserProviderInterface

- (instancetype)init {
  if (self = [super init]) {
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
