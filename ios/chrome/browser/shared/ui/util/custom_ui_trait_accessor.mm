// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/custom_ui_trait_accessor.h"

#import <UIKit/UIKit.h>

@implementation CustomUITraitAccessor

- (instancetype)initWithMutableTraits:(id<UIMutableTraits>)mutableTraits {
  self = [super init];
  if (self) {
    _mutableTraits = mutableTraits;
  }
  return self;
}

@end
