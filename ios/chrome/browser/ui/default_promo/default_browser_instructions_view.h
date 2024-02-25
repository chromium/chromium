// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_INSTRUCTIONS_VIEW_H_
#define IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_INSTRUCTIONS_VIEW_H_

#import <UIKit/UIKit.h>

@protocol ConfirmationAlertActionHandler;

// The accessibility identifier of the default browser instructions view
// animation id.
extern NSString* const kDefaultBrowserInstructionsViewAnimationViewId;

// The accessibility identifier of the default browser instructions view dark
// animation id.
extern NSString* const kDefaultBrowserInstructionsViewDarkAnimationViewId;

// View for the displaying default browser instructions.
@interface DefaultBrowserInstructionsView : UIView

// Creates the view based on provided parameters.
- (instancetype)init:(BOOL)hasDismissButton
            hasSteps:(BOOL)hasSteps
       actionHandler:(id<ConfirmationAlertActionHandler>)actionHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_INSTRUCTIONS_VIEW_H_
