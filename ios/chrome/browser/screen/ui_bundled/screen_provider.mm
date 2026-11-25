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

#pragma mark - Properties

- (std::vector<ScreenType>)userFacingScreens {
  std::vector<ScreenType> userFacingScreens;
  for (ScreenType screen : _screens) {
    if (screen == ScreenType::kStepsCompleted) {
      break;
    }
    userFacingScreens.push_back(screen);
  }
  return userFacingScreens;
}

#pragma mark - Public

- (ScreenType)nextScreenType {
  if (_index >= _screens.size()) {
    return ScreenType::kStepsCompleted;
  }
  return _screens[_index++];
}

- (BOOL)setNextScreenType:(ScreenType)screenType {
  if (screenType == ScreenType::kStepsCompleted) {
    return NO;
  }
  for (size_t i = _index; i < _screens.size(); ++i) {
    if (_screens[i] == ScreenType::kStepsCompleted) {
      break;
    }
    if (_screens[i] == screenType) {
      _index = i;
      return YES;
    }
  }
  return NO;
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
