// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_LEARN_MORE_UI_AGE_MISMATCH_LEARN_MORE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_LEARN_MORE_UI_AGE_MISMATCH_LEARN_MORE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class AgeMismatchLearnMoreViewController;

@class WKWebView;

@protocol AgeMismatchLearnMoreViewControllerDelegate

- (void)ageMismatchLearnMoreViewControllerWantsToBeClosed:
    (AgeMismatchLearnMoreViewController*)viewController;

@end

// View controller used to display the Age Mismatch Learn More content in a web
// view.
@interface AgeMismatchLearnMoreViewController : UIViewController

@property(nonatomic, weak) id<AgeMismatchLearnMoreViewControllerDelegate>
    delegate;

// Initiates a AgeMismatchLearnMoreViewController with
// `webView` with the page in it.
- (instancetype)initWithWebView:(WKWebView*)webView;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_LEARN_MORE_UI_AGE_MISMATCH_LEARN_MORE_VIEW_CONTROLLER_H_
