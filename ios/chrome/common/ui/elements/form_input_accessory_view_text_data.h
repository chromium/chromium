// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_ELEMENTS_FORM_INPUT_ACCESSORY_VIEW_TEXT_DATA_H_
#define IOS_CHROME_COMMON_UI_ELEMENTS_FORM_INPUT_ACCESSORY_VIEW_TEXT_DATA_H_

#import <UIKit/UIKit.h>

// A simple data class to provide `FormInputAcccessoryView` with any necessary
// localized text data. This allows it to be used more easily in the main app
// and extensions.
@interface FormInputAccessoryViewTextData : NSObject

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
           (NSString*)addressManualFillButtonAccessibilityLabel
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Displayed name for keyboard accessory close button.
// Used when the button has no image.
@property(nonatomic, readonly) NSString* closeButtonTitle;
// Accessibility label for keyboard accessory close button.
@property(nonatomic, readonly) NSString* closeButtonAccessibilityLabel;
// Accessibility label for keyboard accessory next button.
@property(nonatomic, readonly) NSString* nextButtonAccessibilityLabel;
// Accessibility label for keyboard accessory previous button.
@property(nonatomic, readonly) NSString* previousButtonAccessibilityLabel;
// Accessibility label for the manual fill keyboard accessory buttons.
// Must be provided if the keyboard accessory has the ability to show
// password and/or autofill suggestions. Can be nil otherwise.
@property(nonatomic, readonly) NSString* manualFillButtonAccessibilityLabel;
@property(nonatomic, readonly)
    NSString* passwordManualFillButtonAccessibilityLabel;
@property(nonatomic, readonly)
    NSString* creditCardManualFillButtonAccessibilityLabel;
@property(nonatomic, readonly)
    NSString* addressManualFillButtonAccessibilityLabel;

@end

#endif  // IOS_CHROME_COMMON_UI_ELEMENTS_FORM_INPUT_ACCESSORY_VIEW_TEXT_DATA_H_
