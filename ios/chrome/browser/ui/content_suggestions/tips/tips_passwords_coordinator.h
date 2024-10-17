// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_PASSWORDS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_PASSWORDS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@class TipsPasswordsCoordinator;
namespace segmentation_platform {
enum class TipIdentifier;
}  // namespace segmentation_platform

// Protocol used to send events from a `TipsPasswordsCoordinator`.
@protocol TipsPasswordsCoordinatorDelegate

// Indicates that the tip finished displaying.
- (void)tipsPasswordsCoordinatorDidFinish:
    (TipsPasswordsCoordinator*)coordinator;

@end

// A coordinator that handles the display of Password-related tips for the Tips
// module.
@interface TipsPasswordsCoordinator
    : ChromeCoordinator <ConfirmationAlertActionHandler,
                         UIAdaptivePresentationControllerDelegate>

// The delegate that receives events from this coordinator.
@property(nonatomic, weak) id<TipsPasswordsCoordinatorDelegate> delegate;

// Creates a coordinator that uses `viewController` and `browser`. Uses
// `identifier` to determine which Passwords tip to show.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                    identifier:(segmentation_platform::TipIdentifier)identifier
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_PASSWORDS_COORDINATOR_H_
