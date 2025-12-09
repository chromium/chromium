// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/public/composebox_theme.h"
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

// Creates an instance with the theme of the input plate.
- (instancetype)initWithTheme:(ComposeboxTheme*)theme;

// The delegate.
@property(nonatomic, weak) id<ComposeboxViewControllerDelegate> delegate;

// The delegate to proxy OmniboxPopupPresenterDelegate calls to.
@property(nonatomic, weak) id<OmniboxPopupPresenterDelegate>
    proxiedPresenterDelegate;

// The close button.
@property(nonatomic, readonly) UIButton* closeButton;

// Container for the omnibox popup.
@property(nonatomic, readonly) UIView* omniboxPopupContainer;

// Adds the input view controller to this ViewController.
- (void)addInputViewController:
    (ComposeboxInputPlateViewController*)inputViewController;

// Requests the input plate to expand beyond to full width when dismissing.
- (void)expandInputPlateForDismissal;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_VIEW_CONTROLLER_H_
