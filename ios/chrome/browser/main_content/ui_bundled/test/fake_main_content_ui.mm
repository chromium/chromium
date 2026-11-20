// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main_content/ui_bundled/test/fake_main_content_ui.h"

@implementation FakeMainContentUI
@synthesize mainContentUIState = _mainContentUIState;

- (instancetype)init {
  if ((self = [super init])) {
    _mainContentUIState = [[TestMainContentUIState alloc] init];
    _stateUpdater =
        [[MainContentUIStateUpdater alloc] initWithState:_mainContentUIState];
  }
  return self;
}

@end
