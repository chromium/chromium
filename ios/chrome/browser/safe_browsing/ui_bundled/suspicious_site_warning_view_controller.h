// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_UI_BUNDLED_SUSPICIOUS_SITE_WARNING_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_UI_BUNDLED_SUSPICIOUS_SITE_WARNING_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class SuspiciousSiteWarningViewController;

// Delegate protocol for handling user actions and external dismissal on
// `SuspiciousSiteWarningViewController`.
@protocol SuspiciousSiteWarningViewControllerDelegate <NSObject>

// Called when the user taps the "Back to safety" primary action button.
- (void)suspiciousSiteWarningViewControllerDidTapBackToSafety:
    (SuspiciousSiteWarningViewController*)viewController;

// Called when the user taps the "Mark as safe" secondary action button.
- (void)suspiciousSiteWarningViewControllerDidTapMarkAsSafe:
    (SuspiciousSiteWarningViewController*)viewController;

// Called when the view controller is dismissed without the user selecting an
// action button (for example, external modal dismissal).
- (void)suspiciousSiteWarningViewControllerDidDismissWithoutAction:
    (SuspiciousSiteWarningViewController*)viewController;

@end

// Modal alert view controller displaying a post-commit Suspicious Site Warning
// with vertically stacked "Back to safety" and "Mark as safe" actions.
@interface SuspiciousSiteWarningViewController : UIViewController

// Delegate notified when the user taps an action button or when the alert is
// dismissed externally.
@property(nonatomic, weak) id<SuspiciousSiteWarningViewControllerDelegate>
    delegate;

// Initializes the warning alert configured for full-screen cross-dissolve modal
// presentation.
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_UI_BUNDLED_SUSPICIOUS_SITE_WARNING_VIEW_CONTROLLER_H_
