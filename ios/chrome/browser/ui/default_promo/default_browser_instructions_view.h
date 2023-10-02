// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_INSTRUCTIONS_VIEW_H_
#define IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_INSTRUCTIONS_VIEW_H_

#import <UIKit/UIKit.h>

@protocol ConfirmationAlertActionHandler;

// View for the displaying default browser instructions.
@interface DefaultBrowserInstructionsView : UIView

// Creates the view based on provided parameters.
- (instancetype)init:(BOOL)hasDismissButton
            hasSteps:(BOOL)hasSteps
       actionHandler:(id<ConfirmationAlertActionHandler>)actionHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_INSTRUCTIONS_VIEW_H_
