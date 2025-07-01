// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_TOS_TOS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_TOS_TOS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class TOSViewController;

@protocol TOSViewControllerPresentationDelegate

- (void)TOSViewControllerWantsToBeClosed:(TOSViewController*)viewController;

@end

// View controller used to display the ToS.
@interface TOSViewController : UIViewController

@property(nonatomic, weak) id<TOSViewControllerPresentationDelegate> delegate;

// Initiates a TOSViewController with
// `TOSView` UIView with ToS page in it;
// `handler` to handle user action.
- (instancetype)initWithContentView:(UIView*)TOSView;

// Closes the TOS.
- (void)close;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_TOS_TOS_VIEW_CONTROLLER_H_
