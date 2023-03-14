// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_UNIFIED_CONSENT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_UNIFIED_CONSENT_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#include <vector>

@class UnifiedConsentCoordinator;
@protocol SystemIdentity;

// Delegate protocol for UnifiedConsentCoordinator.
@protocol UnifiedConsentCoordinatorDelegate<NSObject>

// Called when the user taps on the settings link.
- (void)unifiedConsentCoordinatorDidTapSettingsLink:
    (UnifiedConsentCoordinator*)coordinator;

// Called when the user taps on the 'Learn More' link.
- (void)unifiedConsentCoordinatorDidTapLearnMoreLink:
    (UnifiedConsentCoordinator*)coordinator;

// Called when the user scrolls down to the bottom (or when the view controller
// is loaded with no scroll needed).
- (void)unifiedConsentCoordinatorDidReachBottom:
    (UnifiedConsentCoordinator*)coordinator;

// Called when the user taps on "Add Account…" button.
- (void)unifiedConsentCoordinatorDidTapOnAddAccount:
    (UnifiedConsentCoordinator*)coordinator;

// Called when the primary button needs to update its title (for example if the
// last identity disappears, the button needs to change from "YES, I'M IN" to
// "ADD ACCOUNT").
- (void)unifiedConsentCoordinatorNeedPrimaryButtonUpdate:
    (UnifiedConsentCoordinator*)coordinator;

@end

// UnifiedConsentCoordinator coordinates UnifiedConsentViewController, which is
// a sub view controller to ask for the user consent before the user can
// sign-in.
// All the string ids displayed by the view are available with
// `consentStringIds`. Those can be used to record the consent agreed by the
// user.
@interface UnifiedConsentCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<UnifiedConsentCoordinatorDelegate> delegate;
// Identity selected by the user to sign-in. By default, the identity returned
// by `GetDefaultIdentity()` is used. Must be non-nil if at least one identity
// exists.
@property(nonatomic, strong) id<SystemIdentity> selectedIdentity;
// Informs the coordinator whether the identity picker should automatically be
// open when the UnifiedConsent view appears.
@property(nonatomic) BOOL autoOpenIdentityPicker;
// View controller used to display the view.
@property(nonatomic, strong, readonly) UIViewController* viewController;
// Returns YES if the consent view is scrolled to the bottom.
@property(nonatomic, readonly) BOOL isScrolledToBottom;
// Returns YES if the user tapped on the setting link.
@property(nonatomic, readonly) BOOL settingsLinkWasTapped;
// If YES, the UI elements are disabled.
@property(nonatomic, assign, getter=isUIDisabled) BOOL uiDisabled;
// Returns true if there are policies disabling Sync for at least one data type.
@property(nonatomic, readonly) BOOL hasManagedSyncDataType;
// Returns true if there are account restrictions.
@property(nonatomic, readonly) BOOL hasAccountRestrictions;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Initializes the instance.
// `postRestoreSigninPromoView` should be set to YES, if the dialog is used for
// post restore sign-in promo.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                    postRestoreSigninPromo:(BOOL)postRestoreSigninPromo
    NS_DESIGNATED_INITIALIZER;

// List of string ids used for the user consent. The string ids order matches
// the way they appear on the screen.
- (const std::vector<int>&)consentStringIds;

// Scrolls the consent view to the bottom.
- (void)scrollToBottom;

// Resets settingsLinkWasTapped flag.
- (void)resetSettingLinkTapped;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_UNIFIED_CONSENT_COORDINATOR_H_
