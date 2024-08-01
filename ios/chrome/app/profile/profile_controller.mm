// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/profile_controller.h"

#import "ios/chrome/app/profile/profile_state.h"

@implementation ProfileController

- (instancetype)init {
  if ((self = [super init])) {
    _state = [[ProfileState alloc] init];
  }
  return self;
}

@end
