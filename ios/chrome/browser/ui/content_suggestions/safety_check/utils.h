// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_UTILS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_UTILS_H_

#import <UIKit/UIKit.h>

#import <optional>
#import <vector>

#import "base/time/time.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"

@protocol ApplicationCommands;
@protocol SettingsCommands;
namespace password_manager {
struct CredentialUIEntry;
struct InsecurePasswordCounts;
}  // namespace password_manager
class GURL;
enum class SafetyCheckItemType;
@class SafetyCheckState;

// Fires the proper UI command to navigate users to `chrome_upgrade_url` if the
// app is on a valid channel.
void HandleSafetyCheckUpdateChromeTap(
    const GURL& chrome_upgrade_url,
    id<ApplicationCommands> applicationHandler);

// Fires the proper UI command based on the current `insecure_credentials`
// and `insecure_password_counts`.
void HandleSafetyCheckPasswordTap(
    std::vector<password_manager::CredentialUIEntry>& insecure_credentials,
    password_manager::InsecurePasswordCounts insecure_password_counts,
    id<ApplicationCommands> applicationHandler,
    id<SettingsCommands> settingsHandler);

// Returns true if `state` is considered an invalid state for the Update Chrome
// check in the Safety Check (Magic Stack) module.
bool InvalidUpdateChromeState(UpdateChromeSafetyCheckState state);

// Returns true if `state` is considered an invalid state for the Password
// check in the Safety Check (Magic Stack) module.
bool InvalidPasswordState(PasswordSafetyCheckState state);

// Returns true if `state` is considered an invalid state for the Safe Browsing
// check in the Safety Check (Magic Stack) module.
bool InvalidSafeBrowsingState(SafeBrowsingSafetyCheckState state);

// Returns true if the Safety Check can be run given `last_run_time`.
bool CanRunSafetyCheck(std::optional<base::Time> last_run_time);

// Given `last_run_time`, returns a short, human-readable string for the
// timestamp.
NSString* FormatElapsedTimeSinceLastSafetyCheck(
    std::optional<base::Time> last_run_time);

// Returns the corresponding human-readable name (`NSString*`) for a given
// `item_type`.
NSString* NameForSafetyCheckItemType(SafetyCheckItemType item_type);

// Returns the `SafetyCheckItemType` given a human-readable name (`NSString*`).
SafetyCheckItemType SafetyCheckItemTypeForName(NSString* name);

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_UTILS_H_
