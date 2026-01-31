// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_UI_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_UI_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/autofill/form_input_accessory/ui/form_input_accessory_consumer.h"

@class BrandingViewController;
@protocol FormSuggestionClient;
@class LayoutGuideCenter;
@protocol FormInputAccessoryViewControllerDelegate;

// The maximum number of suggestions to show in the keyboard accessory.
inline constexpr NSUInteger kKeyboardAccessorySuggestionsLimit = 30;

// Creates and manages a custom input accessory view while the user is
// interacting with a form.
@interface FormInputAccessoryViewController
    : UIViewController <FormInputAccessoryConsumer>

// Client in charge of handling actions in suggestions.
@property(nonatomic, weak) id<FormSuggestionClient> formSuggestionClient;

// The view controller to show the branding logo.
@property(nonatomic, strong) BrandingViewController* brandingViewController;

// The layout guide center to use to refer to the first suggestion label.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

// Tells the view to restore the manual fallback icons to a clean state. That
// means no icon selected and the manual fallback view is unlocked.
- (void)reset;

// Instances an object with the desired delegate.
//
// @param FormInputAccessoryViewControllerDelegate the delegate for the actions
// in the manual fallback icons.
// @return A fresh object with the passed delegate.
- (instancetype)initWithFormInputAccessoryViewControllerDelegate:
    (id<FormInputAccessoryViewControllerDelegate>)
        formInputAccessoryViewControllerDelegate;

// Unavailable
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_UI_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_H_
