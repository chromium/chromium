// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_screen_provider.h"

#include "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Screen array.
FirstRunScreenType screens[] = {};
}

@interface FirstRunScreenProvider ()

@property(nonatomic) int index;

@end

@implementation FirstRunScreenProvider

- (instancetype)init {
  self = [super init];
  if (self) {
    SetupScreens();
    _index = -1;
  }
  return self;
}

- (FirstRunScreenType)nextScreenType {
  DCHECK(screens);
  DCHECK(self.index == -1 || screens[self.index] != kFirstRunCompleted);
  return screens[++self.index];
}

#pragma mark - Private
void SetupScreens() {
  // TODO(crbug.com/1195198): Add logic to generate a custimizeed screen
  // order.
  screens[0] = kWelcomeAndConsent;
  screens[1] = kSignIn;
  screens[2] = kSync;
  screens[3] = kFirstRunCompleted;
}

@end
