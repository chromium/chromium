// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_screen_provider.h"

#include "ios/chrome/browser/ui/first_run/fre_field_trial.h"
#import "ios/chrome/browser/ui/screen/screen_provider+protected.h"
#import "ios/chrome/browser/ui/screen/screen_type.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FirstRunScreenProvider

- (instancetype)init {
  NSMutableArray* screens = [NSMutableArray
      arrayWithArray:@[ @(kWelcomeAndConsent), @(kSignInAndSync) ]];

  if (fre_field_trial::IsFREDefaultBrowserScreenEnabled()) {
    [screens addObject:@(kDefaultBrowserPromo)];
  }

  [screens addObject:@(kStepsCompleted)];
  return [super initWithScreens:screens];
}

@end
