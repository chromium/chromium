// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider.h"

@implementation StubBrowserProvider

@synthesize userInteractionEnabled = _userInteractionEnabled;
@synthesize inactiveBrowser = _inactiveBrowser;

- (void)setPrimary:(BOOL)primary {
  // no-op
}

@end
