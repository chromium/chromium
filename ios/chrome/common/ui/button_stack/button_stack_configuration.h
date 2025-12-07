// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_CONFIGURATION_H_
#define IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/util/chrome_button.h"

// Configuration object for a ButtonStackViewController.
@interface ButtonStackConfiguration : NSObject

// The properties for the primary action.
@property(nonatomic, copy) NSString* primaryActionString;
// Defaults to ChromeButtonStylePrimary.
@property(nonatomic, assign) ChromeButtonStyle primaryButtonStyle;
// Defaults to YES.
@property(nonatomic, assign) BOOL primaryActionEnabled;

// The properties for the secondary action.
@property(nonatomic, copy) NSString* secondaryActionString;
// Defaults to nil.
@property(nonatomic, strong) UIImage* secondaryActionImage;
// Defaults to ChromeButtonStyleSecondary.
@property(nonatomic, assign) ChromeButtonStyle secondaryButtonStyle;

// The properties for the tertiary action.
@property(nonatomic, copy) NSString* tertiaryActionString;
// Defaults to ChromeButtonStyleSecondary.
@property(nonatomic, assign) ChromeButtonStyle tertiaryButtonStyle;

// When YES, the primaryActionButton will be disabled, its title text will be
// hidden, and a UIActivityIndicatorView (spinner) will be displayed in its
// place. The secondaryActionButton will also be disabled. When NO, the buttons
// return to their default interactive state. This property is mutually
// exclusive with `isConfirmed`.
@property(nonatomic, assign, getter=isLoading) BOOL loading;

// When YES, the primaryActionButton will be disabled, its title text will be
// hidden, and a checkmark icon will be displayed. This is to provide clear
// visual feedback that the action was completed successfully. This state is
// typically set after an action has completed and is mutually exclusive with
// `isLoading`.
@property(nonatomic, assign, getter=isConfirmed) BOOL confirmed;

// When YES, the entire button stack is hidden. Defaults to NO.
@property(nonatomic, assign) BOOL hideButtons;

@end

#endif  // IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_CONFIGURATION_H_
