// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_IN_PROFILE_PERFORMER_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_IN_PROFILE_PERFORMER_DELEGATE_H_

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer_base_delegate.h"

// Handles completion of AuthenticationFlowPerformerInProfile steps.
@protocol AuthenticationFlowInProfilePerformerDelegate <
    AuthenticationFlowPerformerBaseDelegate>

// Indicates that a profile was signed out, after calling
// `signOutForAccountSwitchWithProfile`.
- (void)didSignOutForAccountSwitch;

// Indicates the account of the user was registered for user policy. `dmToken`
// is empty when registration failed.
- (void)didRegisterForUserPolicyWithDMToken:(NSString*)dmToken
                                   clientID:(NSString*)clientID
                         userAffiliationIDs:
                             (NSArray<NSString*>*)userAffiliationIDs;

// Indicates that account capabilities have been fetched.
- (void)didFetchAccountCapabilities;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_IN_PROFILE_PERFORMER_DELEGATE_H_
