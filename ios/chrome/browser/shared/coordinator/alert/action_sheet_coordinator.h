// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_ALERT_ACTION_SHEET_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_ALERT_ACTION_SHEET_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"

// Coordinator for displaying Action Sheets.
@interface ActionSheetCoordinator : AlertCoordinator

// Init with the parameters for anchoring the popover to a UIView.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                     title:(NSString*)title
                                   message:(NSString*)message
                                      rect:(CGRect)rect
                                      view:(UIView*)view
    NS_DESIGNATED_INITIALIZER;

// Init with the parameters for anchoring the popover to a UIBarButtonItem.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                     title:(NSString*)title
                                   message:(NSString*)message
                             barButtonItem:(UIBarButtonItem*)barButtonItem
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                     title:(NSString*)title
                                   message:(NSString*)message NS_UNAVAILABLE;

// Configures the underlying UIAlertController's popover arrow direction.
// Default is UIPopoverArrowDirectionAny.
@property(nonatomic, assign) UIPopoverArrowDirection popoverArrowDirection;

// Configures the underlying UIAlertController's style. Defaults to
// UIAlertControllerStyleActionSheet.
@property(nonatomic, assign) UIAlertControllerStyle alertStyle;

// Allows replacing title with an attributed string. `updateAttributedText` must
// be called afterward.
@property(nonatomic, copy) NSAttributedString* attributedTitle;
// Allows replacing message with an attributed string. `updateAttributedText`
// must be called afterward.
@property(nonatomic, copy) NSAttributedString* attributedMessage;

// Updates text based on current `attributedTitle` and `attributedMessage`.
- (void)updateAttributedText;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_ALERT_ACTION_SHEET_COORDINATOR_H_
