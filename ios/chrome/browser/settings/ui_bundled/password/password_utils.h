// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_UTILS_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_UTILS_H_

#import <UIKit/UIKit.h>

#import <utility>

#import "base/ios/block_types.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"

namespace password_manager {

// Returns the title and the message for the password alert from an array of
// `origins`. `first`: title and `second`: message.
std::pair<NSString*, NSString*> GetPasswordAlertTitleAndMessageForOrigins(
    NSArray<NSString*>* origins);

// Returns whether any of the affiliated domains of the given credential
// contains the given search term. Expects search term to be in lowercase.
bool MatchCredentialForTerm(const CredentialUIEntry& credential,
                            const std::string& search_term);

// Returns whether branding info or any of the credential groups of the given
// affiliated group matches the given search term. Expects search term to be in
// lowercase.
bool MatchAffiliatedGroupsForTerm(const AffiliatedGroup& affiliated_group,
                                  const std::string& search_term);

// Returns an alert prompting the user to set up a screen lock (passcode, Face
// ID, or Touch ID).
UIAlertController* CreateSetUpScreenLockAlert(
    NSString* message,
    ProceduralBlock learn_how_handler);

}  // namespace password_manager

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_UTILS_H_
