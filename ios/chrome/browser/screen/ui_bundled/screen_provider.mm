// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/screen/ui_bundled/screen_provider.h"

#import <cstddef>
#import <utility>
#import <vector>

@implementation ScreenProvider {
  size_t _index;
  std::vector<ScreenType> _screens;
}

- (ScreenType)nextScreenType {
  if (_index >= _screens.size()) {
    return ScreenType::kStepsCompleted;
  }
  return _screens[_index++];
}

#pragma mark - Private

- (instancetype)initWithScreens:(std::vector<ScreenType>)screens {
  self = [super init];
  if (self) {
    _screens = std::move(screens);
  }
  return self;
}

@end
