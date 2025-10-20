// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"

#import "ios/chrome/common/ui/button_stack/button_stack_constants.h"

@implementation ButtonStackConfiguration

- (instancetype)init {
  self = [super init];
  if (self) {
    _primaryButtonStyle = ButtonStackButtonStylePrimary;
    _secondaryButtonStyle = ButtonStackButtonStyleSecondary;
    _tertiaryButtonStyle = ButtonStackButtonStyleSecondary;
  }
  return self;
}

@end
