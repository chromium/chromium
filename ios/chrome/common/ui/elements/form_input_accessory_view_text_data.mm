// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/elements/form_input_accessory_view_text_data.h"

@implementation FormInputAccessoryViewTextData

- (instancetype)initWithCloseButtonTitle:(NSString*)closeButtonTitle
                   closeButtonAccessibilityLabel:
                       (NSString*)closeButtonAccessibilityLabel
                    nextButtonAccessibilityLabel:
                        (NSString*)nextButtonAccessibilityLabel
                previousButtonAccessibilityLabel:
                    (NSString*)previousButtonAccessibilityLabel
              manualFillButtonAccessibilityLabel:
                  (NSString*)manualFillButtonAccessibilityLabel
      passwordManualFillButtonAccessibilityLabel:
          (NSString*)passwordManualFillButtonAccessibilityLabel
    creditCardManualFillButtonAccessibilityLabel:
        (NSString*)creditCardManualFillButtonAccessibilityLabel
       addressManualFillButtonAccessibilityLabel:
           (NSString*)addressManualFillButtonAccessibilityLabel {
  if ((self = [super init])) {
    _closeButtonTitle = [closeButtonTitle copy];
    _closeButtonAccessibilityLabel = [closeButtonAccessibilityLabel copy];
    _nextButtonAccessibilityLabel = [nextButtonAccessibilityLabel copy];
    _previousButtonAccessibilityLabel = [previousButtonAccessibilityLabel copy];
    _manualFillButtonAccessibilityLabel =
        [manualFillButtonAccessibilityLabel copy];
    _passwordManualFillButtonAccessibilityLabel =
        [passwordManualFillButtonAccessibilityLabel copy];
    _creditCardManualFillButtonAccessibilityLabel =
        [creditCardManualFillButtonAccessibilityLabel copy];
    _addressManualFillButtonAccessibilityLabel =
        [addressManualFillButtonAccessibilityLabel copy];
  }
  return self;
}

@end
