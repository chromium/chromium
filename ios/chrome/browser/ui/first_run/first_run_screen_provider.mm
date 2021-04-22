// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_screen_provider.h"

#include "base/check.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_type.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FirstRunScreenProvider ()

@property(nonatomic, strong) NSArray* screens;
@property(nonatomic) int index;

@end

@implementation FirstRunScreenProvider

- (instancetype)init {
  self = [super init];
  if (self) {
    // Set up screens.
    // Hardcoded default screen order for class initiation.
    // TODO(crbug.com/1195198): Add logic to generate a custimizeed screen
    // order.
    _screens =
        @[ @(kWelcomeAndConsent), @(kSignIn), @(kSync), @(kFirstRunCompleted) ];
    _index = -1;
  }
  return self;
}

- (NSNumber*)nextScreenType {
  DCHECK(self.screens);
  DCHECK(self.index == -1 ||
         ![self.screens[self.index] isEqualToNumber:@(kFirstRunCompleted)]);
  return [self.screens objectAtIndex:++self.index];
}

@end
