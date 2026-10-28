// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_DEFAULT_BROWSER_COORDINATOR_DEFAULT_BROWSER_SCREEN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_DEFAULT_BROWSER_COORDINATOR_DEFAULT_BROWSER_SCREEN_MEDIATOR_H_

#import <Foundation/Foundation.h>

class AuthenticationService;

namespace segmentation_platform {
class DeviceSwitcherResultDispatcher;
}  // namespace segmentation_platform

namespace signin {
class IdentityManager;
}  // namespace signin

@protocol DefaultBrowserScreenConsumer;
@class DefaultBrowserScreenMediator;

// Delegate for `DefaultBrowserScreenMediator`.
@protocol DefaultBrowserScreenMediatorDelegate <NSObject>

// Notifies the delegate whether to present the Android switcher Default Browser
// promo after evaluating eligibility.
- (void)defaultBrowserScreenMediator:(DefaultBrowserScreenMediator*)mediator
    didFinishEvaluationWithAndroidSwitcherPromo:(BOOL)showAndroidSwitcherPromo;

@end

// Mediator for presenting the Default Browser promo in the First Run
// Experience.
@interface DefaultBrowserScreenMediator : NSObject

// Main consumer for this mediator.
@property(nonatomic, weak) id<DefaultBrowserScreenConsumer> consumer;
// Delegate notified once promo evaluation finishes.
@property(nonatomic, weak) id<DefaultBrowserScreenMediatorDelegate> delegate;
// Contains the user choice for UMA reporting. This value is set to the default
// value when the coordinator is initialized.
@property(nonatomic, assign) BOOL UMAReportingUserChoice;
// Whether the user tapped on the TOS link.
@property(nonatomic, assign) BOOL TOSLinkWasTapped;
// Whether the user tapped on the UMA link.
@property(nonatomic, assign) BOOL UMALinkWasTapped;

// Designated initializer with `identityManager`, `authenticationService`, and
// segmentation `dispatcher`.
- (instancetype)
           initWithIdentityManager:(signin::IdentityManager*)identityManager
             authenticationService:(AuthenticationService*)authenticationService
    deviceSwitcherResultDispatcher:
        (segmentation_platform::DeviceSwitcherResultDispatcher*)dispatcher
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Evaluates whether the user is eligible for the Android switcher Default
// Browser promo and notifies `delegate` with the result.
- (void)startPromoEvaluation;

// Disconnects this mediator.
- (void)disconnect;

// Called when the coordinator is finished.
- (void)finishPresenting;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_DEFAULT_BROWSER_COORDINATOR_DEFAULT_BROWSER_SCREEN_MEDIATOR_H_
