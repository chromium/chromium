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
                           manualFillButtonTitle:
                               (NSString*)manualFillButtonTitle
                         atMemoryFullButtonTitle:
                             (NSString*)atMemoryFullButtonTitle
              manualFillButtonAccessibilityLabel:
                  (NSString*)manualFillButtonAccessibilityLabel
      passwordManualFillButtonAccessibilityLabel:
          (NSString*)passwordManualFillButtonAccessibilityLabel
    creditCardManualFillButtonAccessibilityLabel:
        (NSString*)creditCardManualFillButtonAccessibilityLabel
       addressManualFillButtonAccessibilityLabel:
           (NSString*)addressManualFillButtonAccessibilityLabel
      atMemoryManualFillButtonAccessibilityLabel:
          (NSString*)atMemoryManualFillButtonAccessibilityLabel
            atMemoryFullButtonAccessibilityLabel:
                (NSString*)atMemoryFullButtonAccessibilityLabel {
  if ((self = [super init])) {
    _closeButtonTitle = [closeButtonTitle copy];
    _closeButtonAccessibilityLabel = [closeButtonAccessibilityLabel copy];
    _nextButtonAccessibilityLabel = [nextButtonAccessibilityLabel copy];
    _previousButtonAccessibilityLabel = [previousButtonAccessibilityLabel copy];
    _manualFillButtonTitle = [manualFillButtonTitle copy];
    _manualFillButtonAccessibilityLabel =
        [manualFillButtonAccessibilityLabel copy];
    _passwordManualFillButtonAccessibilityLabel =
        [passwordManualFillButtonAccessibilityLabel copy];
    _creditCardManualFillButtonAccessibilityLabel =
        [creditCardManualFillButtonAccessibilityLabel copy];
    _addressManualFillButtonAccessibilityLabel =
        [addressManualFillButtonAccessibilityLabel copy];
    _atMemoryManualFillButtonAccessibilityLabel =
        [atMemoryManualFillButtonAccessibilityLabel copy];
    _atMemoryFullButtonTitle = [atMemoryFullButtonTitle copy];
    _atMemoryFullButtonAccessibilityLabel =
        [atMemoryFullButtonAccessibilityLabel copy];
  }
  return self;
}

@end
