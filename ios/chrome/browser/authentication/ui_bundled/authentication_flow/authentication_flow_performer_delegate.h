// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback_forward.h"
#import "base/ios/block_types.h"
#import "components/policy/core/browser/signin/profile_separation_policies.h"
#import "components/sync/base/data_type.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer_base_delegate.h"
#import "ios/chrome/browser/signin/model/constants.h"

class Browser;
@protocol SystemIdentity;
@class UIViewController;

// Handles completion of AuthenticationFlowPerformerBase steps.
@protocol AuthenticationFlowPerformerDelegate <
    AuthenticationFlowPerformerBaseDelegate>

// Called after `-[AuthenticationFlowPerformerBase
// fetchUnsyncedDataWithSyncService:]`, to return the list of data types
// unsynced in the current profile.
- (void)didFetchUnsyncedDataWithUnsyncedDataTypes:
    (syncer::DataTypeSet)unsyncedDataTypes;

// Called once the user accepts or refuses to leave the primary account.
// See `-[AuthenticationFlowPerformerBase
// showLeavingPrimaryAccountConfirmationWithBaseViewController:browser:
// anchorView:anchorRect:]`.
- (void)didAcceptToLeavePrimaryAccount:(BOOL)acceptToContinue;

// Indicates that the identity managed status was fetched.
- (void)didFetchManagedStatus:(NSString*)hostedDomain;

// Indicates that the requested identity managed status fetch failed.
- (void)didFailFetchManagedStatus:(NSError*)error;

// Indicates that the value for ProfileSeparationDataMigrationSettings has been
// fetched from the server.
- (void)didFetchProfileSeparationPolicies:
    (policy::ProfileSeparationDataMigrationSettings)
        profileSeparationDataMigrationSettings;

// Indicates that the user accepted signing in to a managed account.
// If `browsingDataSeparate` is `YES`, the managed account gets signed in to
// a new empty work profile. This must only be specified if
// AreSeparateProfilesForManagedAccountsEnabled() is true.
// If `browsingDataSeparate` is `NO`, the account gets signed in to the
// current profile. If AreSeparateProfilesForManagedAccountsEnabled() is true,
// this involves converting the current profile into a work profile.
- (void)didAcceptManagedConfirmationWithBrowsingDataSeparate:
    (BOOL)browsingDataSeparate;

// Indicates that the user cancelled signing in to a managed account.
- (void)didCancelManagedConfirmation;

// Indicates that switching to a different profile failed.
- (void)didFailToSwitchToProfile;

// Indicates that the personal profile was converted to a managed one.
- (void)didMakePersonalProfileManaged;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_DELEGATE_H_
