// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_H_

#import <UIKit/UIKit.h>

#import "components/policy/core/browser/signin/profile_separation_policies.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer_base.h"
#import "ios/chrome/browser/signin/model/constants.h"

@protocol AuthenticationFlowDelegate;
class ProfileIOS;
namespace signin_metrics {
enum class AccessPoint;
}
enum class SignedInUserState;
namespace signin {
enum class ManagedAccountSigninMode;
}  // namespace signin
@protocol SystemIdentity;
namespace syncer {
class SyncService;
}

// Performs the sign-in steps and user interactions as part of the sign-in in
// same profile flow.
@interface AuthenticationFlowPerformer : AuthenticationFlowPerformerBase

// Cancels any outstanding work and dismisses an alert view (if shown).
- (void)interrupt;

// Starts the reauthentication flow for `identity`.
- (void)reauthIdentity:(id<SystemIdentity>)identity
               browser:(Browser*)browser
        viewController:(UIViewController*)viewController
           accessPoint:(signin_metrics::AccessPoint)accessPoint;

// Fetches the list of data types with unsync data in the primary account.
// `-[id<AuthenticationFlowPerformerDelegate>
// didFetchUnsyncedDataWithUnsyncedDataTypes:]` is called once the data are
// fetched.
- (void)fetchUnsyncedDataWithSyncService:(syncer::SyncService*)syncService;

// Shows confirmation dialog to leaving the primary account. This dialog
// is used for account switching or profile switching.
// `baseViewController` is used to display the confirmation diolog.
// `anchorView` and `anchorRect` is the position that triggered sign-in. It is
// used to attach the popover dialog with a regular window size (like iPad).
// `-[id<AuthenticationFlowPerformerDelegate>
// didAcceptToLeavePrimaryAccount:]` is called once the user accepts or
// refuses the confirmation dialog.
- (void)showLeavingPrimaryAccountConfirmationWithBaseViewController:
            (UIViewController*)baseViewController
                                                            browser:(Browser*)
                                                                        browser
                                                  signedInUserState:
                                                      (SignedInUserState)
                                                          signedInUserState
                                                         anchorView:
                                                             (UIView*)anchorView
                                                         anchorRect:
                                                             (CGRect)anchorRect;

// Fetches the managed status for `identity`.
- (void)fetchManagedStatus:(ProfileIOS*)profile
               forIdentity:(id<SystemIdentity>)identity;

// Starts fetching capability to determine if the user can sign in to Chrome.
- (void)fetchCanSignInToChromeCapability:(id<SystemIdentity>)identity
                                 profile:(ProfileIOS*)profile;

// Shows the Age Mismatch dialog.
- (void)showAgeMismatchDialogForIdentity:(id<SystemIdentity>)identity
                          viewController:(UIViewController*)viewController
                                 browser:(Browser*)browser;

// Fetches the profile separation policies for the account linked to `identity`.
- (void)fetchProfileSeparationPolicies:(ProfileIOS*)profile
                           forIdentity:(id<SystemIdentity>)identity;

// Switches to the profile that `identity` is assigned, for `sceneIdentifier`.
// The delegate’s command must be called before the change of profile.
// ChangeProfileContinuationProvider is a base::RepeatingCallback, which is not
// compatible with unit tests mocks. This is why this method use a protocol as
// argument instead.
- (void)switchToProfileWithIdentity:(id<SystemIdentity>)identity
                         sceneState:(SceneState*)sceneState
                             reason:(ChangeProfileReason)reason
                           delegate:(id<AuthenticationFlowDelegate>)delegate
                  postSignInActions:(PostSignInActionSet)postSignInActions
                        accessPoint:(signin_metrics::AccessPoint)accessPoint;

// Converts the personal profile to a managed one and attaches `identity` to it.
- (void)makePersonalProfileManagedWithIdentity:(id<SystemIdentity>)identity;

// Shows a confirmation dialog for signing in to an account managed by
// `hostedDomain`. The confirmation dialog's content will be different depending
// on the status of User Policy.
- (void)showManagedConfirmationForHostedDomain:(NSString*)hostedDomain
                                      identity:(id<SystemIdentity>)identity
                                viewController:(UIViewController*)viewController
                                       browser:(Browser*)browser
                    managedProfileCreationMode:
                        (signin::ManagedAccountSigninMode)mode;

@end

@interface AuthenticationFlowPerformer (ForTesting)

// If `useFakeResponses` is true, policy requests will be immediately answered,
// with a default "no policy" response, instead of via a network request to the
// policy server. If a non-default response is required, use
// `forcePolicyResponseForNextRequestForTesting` instead.
// Any test that uses this should reset it to `false` at the end of the test.
+ (void)setUseFakePolicyResponsesForTesting:(BOOL)useFakeResponses;

// Forces the ProfileSeparationDataMigrationSettings value for the next request
// made to fetch ProfileSeparationPolicies.
+ (void)forcePolicyResponseForNextRequestForTesting:
    (policy::ProfileSeparationDataMigrationSettings)
        profileSeparationDataMigrationSettings;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_H_
