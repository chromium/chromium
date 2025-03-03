// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "components/policy/core/browser/signin/profile_separation_policies.h"
#import "components/sync/base/data_type.h"
#import "ios/chrome/browser/signin/model/constants.h"

@class UIViewController;

// Handles completion of AuthenticationFlowPerformer steps.
@protocol AuthenticationFlowPerformerDelegate <NSObject>

// Indicates that a profile was signed out, after calling
// `signOutForAccountSwitchWithProfile`.
- (void)didSignOutForAccountSwitch;

// Indicates that browsing data finished clearing.
- (void)didClearData;

// Called after `-[AuthenticationFlowPerformer
// fetchUnsyncedDataWithSyncService:]`, to return the list of data types
// unsynced in the current profile.
- (void)didFetchUnsyncedDataWithUnsyncedDataTypes:
    (syncer::DataTypeSet)unsyncedDataTypes;

// Called once the user accepts or refuses to leave the primary account.
// See `-[AuthenticationFlowPerformer
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
- (void)didAcceptManagedConfirmation:(BOOL)keepBrowsingDataSeparate;

// Indicates that the user cancelled signing in to a managed account.
- (void)didCancelManagedConfirmation;

// Indicates the account of the user was registered for user policy. `dmToken`
// is empty when registration failed.
- (void)didRegisterForUserPolicyWithDMToken:(NSString*)dmToken
                                   clientID:(NSString*)clientID
                         userAffiliationIDs:
                             (NSArray<NSString*>*)userAffiliationIDs;

// Indicates that user policies were fetched. `success` is true when the fetch
// was successful.
- (void)didFetchUserPolicyWithSuccess:(BOOL)success;

// Indicates that the personal profile was converted to a managed one.
- (void)didMakePersonalProfileManaged;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_DELEGATE_H_
