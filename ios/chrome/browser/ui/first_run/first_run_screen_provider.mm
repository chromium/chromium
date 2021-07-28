// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_screen_provider.h"

#include "base/check.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FirstRunScreenProvider ()

@property(nonatomic, assign) NSInteger index;

@property(nonatomic, strong) NSMutableArray* screens;

@end

@implementation FirstRunScreenProvider

- (instancetype)init {
  self = [super init];
  if (self) {
    [self setupScreens];
    _index = -1;
  }
  return self;
}

- (FirstRunScreenType)nextScreenType {
  DCHECK(self.screens);
  DCHECK(self.index == -1 ||
         ![self.screens[self.index] isEqual:@(kFirstRunCompleted)]);
  return static_cast<FirstRunScreenType>(
      [self.screens[++self.index] integerValue]);
}

#pragma mark - Private

// Sets the screens up.
- (void)setupScreens {
  self.screens = [NSMutableArray array];
  [self.screens addObject:@(kWelcomeAndConsent)];
  [self.screens addObject:@(kSignIn)];
  [self.screens addObject:@(kSync)];

  if (base::FeatureList::IsEnabled(kEnableFREDefaultBrowserScreen)) {
    [self.screens addObject:@(kDefaultBrowserPromo)];
  }

  [self.screens addObject:@(kFirstRunCompleted)];
}

@end
