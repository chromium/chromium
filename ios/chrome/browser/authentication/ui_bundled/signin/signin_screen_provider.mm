// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_screen_provider.h"

#import "ios/chrome/browser/screen/ui_bundled/screen_provider+protected.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"

@implementation SigninScreenProvider

- (instancetype)init {
  NSMutableArray* screens = [NSMutableArray array];
  [screens addObject:@(kSignIn)];
  [screens addObject:@(kStepsCompleted)];
  return [super initWithScreens:screens];
}

@end
