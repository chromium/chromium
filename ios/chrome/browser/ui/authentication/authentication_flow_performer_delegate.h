// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_FLOW_PERFORMER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_FLOW_PERFORMER_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#include "ios/chrome/browser/signin/model/constants.h"

@class UIViewController;

// Handles completion of AuthenticationFlowPerformer steps.
@protocol AuthenticationFlowPerformerDelegate<NSObject>

// Indicates that a profile was signed out.
- (void)didSignOut;

// Indicates that browsing data finished clearing.
- (void)didClearData;

// Indicates that the identity managed status was fetched.
- (void)didFetchManagedStatus:(NSString*)hostedDomain;

// Indicates that the requested identity managed status fetch failed.
- (void)didFailFetchManagedStatus:(NSError*)error;

// Indicates that the user accepted signing in to a managed account.
- (void)didAcceptManagedConfirmation;

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

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_FLOW_PERFORMER_DELEGATE_H_
