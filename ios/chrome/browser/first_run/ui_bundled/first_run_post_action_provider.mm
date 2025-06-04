// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/first_run_post_action_provider.h"

#import "ios/chrome/browser/passwords/model/features.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_provider+protected.h"
#import "ios/chrome/browser/shared/public/features/features.h"

@implementation FirstRunPostActionProvider

- (instancetype)init {
  NSMutableArray<NSNumber*>* screens = [NSMutableArray array];
  if (IsBestOfAppGuidedTourEnabled()) {
    [screens addObject:@(kGuidedTour)];
  }
  if (base::FeatureList::IsEnabled(kImportPasswordsFromSafari)) {
    [screens addObject:@(kSafariImport)];
  }
  [screens addObject:@(kStepsCompleted)];
  return [super initWithScreens:screens];
}

@end
