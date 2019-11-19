// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/elements/text_field_configuration.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TextFieldConfiguration

- (instancetype)initWithText:(NSString*)text
                 placeholder:(NSString*)placeholder
     accessibilityIdentifier:(NSString*)accessibilityIdentifier
             secureTextEntry:(BOOL)secureTextEntry {
  self = [super init];
  if (self) {
    _text = [text copy];
    _placeholder = [placeholder copy];
    _accessibilityIdentifier = [accessibilityIdentifier copy];
    _secureTextEntry = secureTextEntry;
  }
  return self;
}

@end
