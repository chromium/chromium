// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/test/stub_browser_interface_provider.h"

#import "ios/chrome/browser/ui/main/test/stub_browser_interface.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation StubBrowserInterfaceProvider

- (instancetype)init {
  if (self = [super init]) {
    _mainInterface = [[StubBrowserInterface alloc] init];
    _incognitoInterface = [[StubBrowserInterface alloc] init];
    _incognitoInterface.incognito = YES;
    _currentInterface = _mainInterface;
  }
  return self;
}

- (void)cleanDeviceSharingManager {
  // no-op
}
@end
