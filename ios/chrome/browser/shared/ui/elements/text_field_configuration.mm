// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/text_field_configuration.h"

@implementation TextFieldConfiguration

- (instancetype)initWithText:(NSString*)text
                 placeholder:(NSString*)placeholder
     accessibilityIdentifier:(NSString*)accessibilityIdentifier
      autocapitalizationType:
          (UITextAutocapitalizationType)autocapitalizationType
             secureTextEntry:(BOOL)secureTextEntry {
  self = [super init];
  if (self) {
    _text = [text copy];
    _placeholder = [placeholder copy];
    _accessibilityIdentifier = [accessibilityIdentifier copy];
    _autocapitalizationType = autocapitalizationType;
    _secureTextEntry = secureTextEntry;
  }
  return self;
}

@end
