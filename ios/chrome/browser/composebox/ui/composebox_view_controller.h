// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/ui/composebox_navigation_consumer.h"
#import "ios/chrome/browser/omnibox/ui/popup/omnibox_popup_presenter.h"

@class ComposeboxInputPlateViewController;
@class ComposeboxViewController;

// Delegate for the container view controller.
@protocol ComposeboxViewControllerDelegate
- (void)composeboxViewControllerDidTapCloseButton:
    (ComposeboxViewController*)viewController;
@end

// View Controller that contains the composebox, presenting it modally.
@interface ComposeboxViewController
    : UIViewController <ComposeboxNavigationConsumer,
                        OmniboxPopupPresenterDelegate>

// The delegate.
@property(nonatomic, weak) id<ComposeboxViewControllerDelegate> delegate;

// Adds the input view controller to this ViewController.
- (void)addInputViewController:
    (ComposeboxInputPlateViewController*)inputViewController;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_VIEW_CONTROLLER_H_
