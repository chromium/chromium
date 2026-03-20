// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/coordinator/fullscreen_mediator.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"

@implementation FullscreenMediator {
  raw_ptr<FullscreenBrowserAgent> _browserAgent;
}

#pragma mark - Public

- (instancetype)initWithBrowserAgent:(FullscreenBrowserAgent*)browserAgent {
  if ((self = [super init])) {
    _browserAgent = browserAgent;
  }
  return self;
}

- (void)disconnect {
  _browserAgent = nullptr;
}
@end
