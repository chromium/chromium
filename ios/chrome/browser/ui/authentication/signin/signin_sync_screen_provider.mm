// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/signin_sync_screen_provider.h"

#import "ios/chrome/browser/ui/screen/screen_provider+protected.h"
#import "ios/chrome/browser/ui/screen/screen_type.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SigninSyncScreenProvider

- (instancetype)init {
  NSArray<NSNumber*>* screens =
      @[ @(kSignIn), @(kTangibleSync), @(kStepsCompleted) ];
  return [super initWithScreens:screens];
}

@end
