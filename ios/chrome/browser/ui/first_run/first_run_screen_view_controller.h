// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/first_run/first_run_screen_view_controller_delegate.h"

// A base view controller for the common UI controls in the new First Run
// Experience screens.
@interface FirstRunScreenViewController : UIViewController

// The banner image. Must be set before the view is loaded.
@property(nonatomic, strong) UIImage* bannerImage;

// When set to YES, the banner will be tall (35% of view height). When set to
// NO, the banner will be of normal height (25% of view height). Defaults to NO.
@property(nonatomic, assign) BOOL isTallBanner;

// The label of the headline below the image. Must be set before the view is
// loaded. This is declared public so the accessibility can be enabled.
@property(nonatomic, strong) UILabel* titleLabel;

// The headline below the image. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* titleText;

// The subtitle below the title. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* subtitleText;

// The container view for the screen-specific content. Derived view controllers
// should add their UI elements to it.
@property(nonatomic, strong) UIView* specificContentView;

// The text for the primary action. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* primaryActionString;

// The text for the secondary action. Must be set before the view is loaded. If
// not set, there won't be a secondary action button.
@property(nonatomic, copy) NSString* secondaryActionString;

// The text for the tertiary action. Must be set before the view is loaded. If
// not set, there won't be a tertiary action button.
@property(nonatomic, copy) NSString* tertiaryActionString;

// The delegate to invoke when buttons are tapped. Can be derived by screen-
// specific view controllers if additional buttons are used.
@property(nonatomic, weak) id<FirstRunScreenViewControllerDelegate> delegate;

// When set to YES, the primary button is temporarily replaced with a "More"
// button that scrolls the content, until the user scrolls to the very end of
// the content. If set to NO, the primary button behaves normally. Defaults to
// NO.
@property(nonatomic, assign) BOOL scrollToEndMandatory;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_VIEW_CONTROLLER_H_
