// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_screen_provider.h"

#import "base/notreached.h"
#import "ios/chrome/browser/ui/screen/screen_provider+protected.h"
#import "ios/chrome/browser/ui/screen/screen_type.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FirstRunScreenProvider

- (instancetype)init {
  NSMutableArray* screens = [NSMutableArray array];
  [screens addObject:@(kSignIn)];
  [screens addObject:@(kTangibleSync)];
  [screens addObject:@(kDefaultBrowserPromo)];

  if (ios::provider::IsChoiceEnabled()) {
    [screens addObject:@(kChoice)];
  }

  [screens addObject:@(kStepsCompleted)];
  return [super initWithScreens:screens];
}

@end
