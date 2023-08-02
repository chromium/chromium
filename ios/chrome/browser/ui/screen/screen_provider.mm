// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/screen/screen_provider.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"

@interface ScreenProvider ()

// Index in the array of screens currently shown.
@property(nonatomic, assign) NSInteger index;

// Array of screens to display to the user.
@property(nonatomic, strong) NSArray* screens;

@end

@implementation ScreenProvider

- (ScreenType)nextScreenType {
  DCHECK(self.screens);
  DCHECK(self.index == -1 ||
         ![self.screens[self.index] isEqual:@(kStepsCompleted)]);
  return static_cast<ScreenType>([self.screens[++self.index] integerValue]);
}

#pragma mark - Private

- (instancetype)initWithScreens:(NSArray*)screens {
  self = [super init];
  if (self) {
    _screens = screens;
    _index = -1;
  }
  return self;
}

@end
