// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_UNIFIED_CONSENT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_UNIFIED_CONSENT_COORDINATOR_H_

#import <UIKit/UIKit.h>

#include <vector>

@class ChromeIdentity;
@class UnifiedConsentCoordinator;

// Delegate protocol for UnifiedConsentCoordinator.
@protocol UnifiedConsentCoordinatorDelegate<NSObject>

// Called when the user taps on the settings link.
- (void)unifiedConsentCoordinatorDidTapSettingsLink:
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
// |consentStringIds| and |openSettingsStringId|. Those can be used to record
// the consent agreed by the user.
@interface UnifiedConsentCoordinator : NSObject

@property(nonatomic, weak) id<UnifiedConsentCoordinatorDelegate> delegate;
// Identity selected by the user to sign-in. By default, the first identity from
// GetAllIdentitiesSortedForDisplay() is used.
// Must be non-nil if at least one identity exists.
@property(nonatomic, strong) ChromeIdentity* selectedIdentity;
// Informs the coordinator whether the identity picker should automatically be
// open when the UnifiedConsent view appears.
@property(nonatomic) BOOL autoOpenIdentityPicker;
// String id for text to open the settings (related to record the user consent).
@property(nonatomic, readonly) int openSettingsStringId;
// View controller used to display the view.
@property(nonatomic, strong, readonly) UIViewController* viewController;
// Returns YES if the consent view is scrolled to the bottom.
@property(nonatomic, readonly) BOOL isScrolledToBottom;
// Returns YES if the user tapped on the setting link.
@property(nonatomic, readonly) BOOL settingsLinkWasTapped;
// If YES, the UI elements are disabled.
// TODO(crbug.com/1003737): This should be implemented with
// ActivityOverlayCoordinator when all the cleanup will be done in
// ChromeSigninViewController.
@property(nonatomic, assign, getter=isUIDisabled) BOOL uiDisabled;

// Starts this coordinator.
- (void)start;

// List of string ids used for the user consent. The string ids order matches
// the way they appear on the screen.
- (const std::vector<int>&)consentStringIds;

// Scrolls the consent view to the bottom.
- (void)scrollToBottom;

// Resets settingsLinkWasTapped flag.
- (void)resetSettingLinkTapped;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_UNIFIED_CONSENT_COORDINATOR_H_
