// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/ui/passkey_welcome_screen_strings.h"

@implementation PasskeyWelcomeScreenStrings

- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                       footer:(NSString*)footer
                primaryButton:(NSString*)primaryButton
              secondaryButton:(NSString*)secondaryButton
                 instructions:(NSArray<NSString*>*)instructions {
  self = [super init];
  if (self) {
    _title = title;
    _subtitle = subtitle;
    _footer = footer;
    _primaryButton = primaryButton;
    _secondaryButton = secondaryButton;
    _instructions = instructions;
  }
  return self;
}

@end
