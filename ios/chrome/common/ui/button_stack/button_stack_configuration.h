// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_CONFIGURATION_H_
#define IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/button_stack/button_stack_constants.h"

// Configuration object for a ButtonStackViewController.
@interface ButtonStackConfiguration : NSObject

// The properties for the primary action.
@property(nonatomic, copy) NSString* primaryActionString;
@property(nonatomic, assign) ButtonStackButtonStyle primaryButtonStyle;
@property(nonatomic, strong) UIImage* primaryActionImage;

// The properties for the secondary action.
@property(nonatomic, copy) NSString* secondaryActionString;
@property(nonatomic, assign) ButtonStackButtonStyle secondaryButtonStyle;
@property(nonatomic, strong) UIImage* secondaryActionImage;

// The properties for the tertiary action.
@property(nonatomic, copy) NSString* tertiaryActionString;
@property(nonatomic, assign) ButtonStackButtonStyle tertiaryButtonStyle;
@property(nonatomic, strong) UIImage* tertiaryActionImage;

// When YES, the primaryActionButton will be disabled, its title text will be
// hidden, and a UIActivityIndicatorView (spinner) will be displayed in its
// place. The secondaryActionButton will also be disabled. When NO, the buttons
// return to their default interactive state.
@property(nonatomic, assign) BOOL isLoading;

// When YES, the primaryActionButton will be disabled, its title text will be
// hidden, and a checkmark icon will be displayed. This is to provide clear
// visual feedback that the action was completed successfully. This state is
// typically set after an action has completed and is mutually exclusive with
// isLoading.
@property(nonatomic, assign) BOOL isConfirmed;

@end

#endif  // IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_CONFIGURATION_H_
