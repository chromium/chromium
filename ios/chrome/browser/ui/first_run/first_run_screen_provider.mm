// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_screen_provider.h"

#import "base/notreached.h"
#import "ios/chrome/browser/ui/first_run/fre_field_trial.h"
#import "ios/chrome/browser/ui/screen/screen_provider+protected.h"
#import "ios/chrome/browser/ui/screen/screen_type.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FirstRunScreenProvider

- (instancetype)init {
  NSMutableArray* screens = [NSMutableArray array];

  switch (fre_field_trial::GetNewMobileIdentityConsistencyFRE()) {
    case NewMobileIdentityConsistencyFRE::kTwoSteps:
      [screens addObject:@(kSignIn)];
      [screens addObject:@(kSync)];
      break;
    case NewMobileIdentityConsistencyFRE::kTangibleSyncA:
    case NewMobileIdentityConsistencyFRE::kTangibleSyncB:
    case NewMobileIdentityConsistencyFRE::kTangibleSyncC:
    case NewMobileIdentityConsistencyFRE::kTangibleSyncD:
    case NewMobileIdentityConsistencyFRE::kTangibleSyncE:
    case NewMobileIdentityConsistencyFRE::kTangibleSyncF:
      [screens addObject:@(kSignIn)];
      [screens addObject:@(kTangibleSync)];
      break;
    case NewMobileIdentityConsistencyFRE::kOld:
      [screens addObject:@(kWelcomeAndConsent)];
      [screens addObject:@(kSignInAndSync)];
      break;
  }

  if (fre_field_trial::GetFREDefaultBrowserScreenPromoFRE() !=
      NewDefaultBrowserPromoFRE::kDisabled) {
    [screens addObject:@(kDefaultBrowserPromo)];
  }

  [screens addObject:@(kStepsCompleted)];
  return [super initWithScreens:screens];
}

@end
