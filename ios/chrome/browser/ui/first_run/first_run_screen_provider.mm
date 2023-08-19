// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_screen_provider.h"

#import "base/feature_list.h"
#import "base/notreached.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/ui/screen/screen_provider+protected.h"
#import "ios/chrome/browser/ui/screen/screen_type.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"

@implementation FirstRunScreenProvider

- (instancetype)init {
  NSMutableArray* screens = [NSMutableArray array];
  [screens addObject:@(kSignIn)];
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    [screens addObject:@(kHistorySync)];
  } else {
    [screens addObject:@(kTangibleSync)];
  }
  [screens addObject:@(kDefaultBrowserPromo)];

  if (ios::provider::IsChoiceEnabled()) {
    [screens addObject:@(kChoice)];
  }

  [screens addObject:@(kStepsCompleted)];
  return [super initWithScreens:screens];
}

@end
