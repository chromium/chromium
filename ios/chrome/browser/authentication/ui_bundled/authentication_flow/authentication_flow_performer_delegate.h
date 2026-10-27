// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "components/policy/core/browser/signin/profile_separation_policies.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/base/data_type.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer_base_delegate.h"
#import "ios/chrome/browser/signin/model/constants.h"

class Browser;
@protocol SystemIdentity;
@class UIViewController;
@class AuthenticationFlowPerformer;

// Handles completion of AuthenticationFlowPerformer steps.
@protocol AuthenticationFlowPerformerDelegate <
    AuthenticationFlowPerformerBaseDelegate>

// Indicates that the reauthentication flow completed.
- (void)authenticationFlowPerformer:(AuthenticationFlowPerformer*)performer
       didCompleteReauthWithSuccess:(BOOL)success;

// Indicates that the CanSignInToChrome capability was fetched.
- (void)authenticationFlowPerformer:(AuthenticationFlowPerformer*)performer
    didFetchCanSignInToChromeCapability:(signin::Tribool)capability;

// Called after `-[AuthenticationFlowPerformer
// fetchUnsyncedDataWithSyncService:]`, to return the list of data types
// unsynced in the current profile.
- (void)authenticationFlowPerformer:(AuthenticationFlowPerformer*)performer
    didFetchUnsyncedDataWithUnsyncedDataTypes:
        (syncer::DataTypeSet)unsyncedDataTypes;

// Called once the user accepts or refuses to leave the primary account.
// See `-[AuthenticationFlowPerformer
// showLeavingPrimaryAccountConfirmationWithBaseViewController:browser:
// anchorView:anchorRect:]`.
- (void)authenticationFlowPerformer:(AuthenticationFlowPerformer*)performer
     didAcceptToLeavePrimaryAccount:(BOOL)acceptToContinue;

// Indicates that the identity managed status was fetched.
- (void)authenticationFlowPerformer:(AuthenticationFlowPerformer*)performer
              didFetchManagedStatus:(NSString*)hostedDomain;

// Indicates that the requested identity managed status fetch failed.
- (void)authenticationFlowPerformer:(AuthenticationFlowPerformer*)performer
          didFailFetchManagedStatus:(NSError*)error;

// Indicates that the Age Mismatch dialog was dismissed.
- (void)authenticationFlowPerformer:(AuthenticationFlowPerformer*)performer
    didDismissAgeMismatchDialogWithCancelationReason:
        (signin_ui::CancelationReason)reason;

// Indicates that the value for ProfileSeparationDataMigrationSettings has been
// fetched from the server.
- (void)authenticationFlowPerformer:(AuthenticationFlowPerformer*)performer
    didFetchProfileSeparationPolicies:
        (policy::ProfileSeparationDataMigrationSettings)
            profileSeparationDataMigrationSettings;

// Indicates that the user accepted signing in to a managed account.
// If `browsingDataSeparate` is `YES`, the managed account gets signed in to a
// new empty work profile. If `browsingDataSeparate` is `NO`, the account gets
// signed in to the current profile, converting it into a work profile.
- (void)authenticationFlowPerformer:(AuthenticationFlowPerformer*)performer
    didAcceptManagedConfirmationWithBrowsingDataSeparate:
        (BOOL)browsingDataSeparate;

// Indicates that the user cancelled signing in to a managed account.
- (void)authenticationFlowPerformerDidCancelManagedConfirmation:
    (AuthenticationFlowPerformer*)performer;

// Indicates that the managed confirmation could not proceed.
- (void)authenticationFlowPerformerDidFailToProceedWithManagedConfirmation:
    (AuthenticationFlowPerformer*)performer;

// Indicates that the personal profile was converted to a managed one.
- (void)authenticationFlowPerformerDidMakePersonalProfileManaged:
    (AuthenticationFlowPerformer*)performer;

// Indicates whether the user confirmed changing profile can proceed.
- (void)authenticationFlowPerformer:(AuthenticationFlowPerformer*)performer
    didConfirmChangeProfileCanProceed:(BOOL)canProceed;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_DELEGATE_H_
