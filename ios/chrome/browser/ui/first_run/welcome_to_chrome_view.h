// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_TO_CHROME_VIEW_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_TO_CHROME_VIEW_H_

#import <UIKit/UIKit.h>

@class WelcomeToChromeView;

// Delegate for WelcomeToChromeViews.
@protocol WelcomeToChromeViewDelegate<NSObject>

// Called when the user taps on the "Terms of Service" link.
- (void)welcomeToChromeViewDidTapTOSLink;

// Called when the user taps the "Accept & Continue" button.
- (void)welcomeToChromeViewDidTapOKButton:(WelcomeToChromeView*)view;

@end

// The first view shown to the user after fresh installs.
@interface WelcomeToChromeView : UIView

@property(nonatomic, weak) id<WelcomeToChromeViewDelegate> delegate;

// Whether the stats reporting check box is selected.
@property(nonatomic, assign, getter=isCheckBoxSelected) BOOL checkBoxSelected;

// Runs the transition animation from Launch Screen to the Welcome to Chrome
// View. This method must be called after the view is added to a superview and
// the view's subviews have been laid out.
- (void)runLaunchAnimation;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_TO_CHROME_VIEW_H_
