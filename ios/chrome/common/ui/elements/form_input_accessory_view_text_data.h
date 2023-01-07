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

- (instancetype)
            initWithCloseButtonTitle:(NSString*)closeButtonTitle
       closeButtonAccessibilityLabel:(NSString*)closeButtonAccessibilityLabel
        nextButtonAccessibilityLabel:(NSString*)nextButtonAccessibilityLabel
    previousButtonAccessibilityLabel:(NSString*)previousButtonAccessibilityLabel
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, readonly) NSString* closeButtonTitle;
@property(nonatomic, readonly) NSString* closeButtonAccessibilityLabel;
@property(nonatomic, readonly) NSString* nextButtonAccessibilityLabel;
@property(nonatomic, readonly) NSString* previousButtonAccessibilityLabel;

@end

#endif  // IOS_CHROME_COMMON_UI_ELEMENTS_FORM_INPUT_ACCESSORY_VIEW_TEXT_DATA_H_
