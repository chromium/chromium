// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"

@implementation ButtonStackConfiguration

- (instancetype)init {
  self = [super init];
  if (self) {
    _primaryButtonStyle = ChromeButtonStylePrimary;
    _primaryActionEnabled = YES;
    _secondaryButtonStyle = ChromeButtonStyleSecondary;
    _tertiaryButtonStyle = ChromeButtonStyleSecondary;
  }
  return self;
}

@end
