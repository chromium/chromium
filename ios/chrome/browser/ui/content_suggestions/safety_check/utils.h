// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_UTILS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_UTILS_H_

#import <vector>

#import <UIKit/UIKit.h>

#import "base/time/time.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

@protocol ApplicationCommands;
namespace password_manager {
struct CredentialUIEntry;
}  // namespace password_manager
class GURL;
@class SafetyCheckState;

// Fires the proper UI command to navigate users to `chrome_upgrade_url` if the
// app is on a valid channel.
void HandleSafetyCheckUpdateChromeTap(const GURL& chrome_upgrade_url,
                                      id<ApplicationCommands> handler);

// Fires the proper UI command based on the current compromised credentials
// list, `credentials`.
void HandleSafetyCheckPasswordTap(
    std::vector<password_manager::CredentialUIEntry>& credentials,
    id<ApplicationCommands> handler);

// Returns true if `state` is considered an invalid state for the Update Chrome
// check in the Safety Check (Magic Stack) module.
bool InvalidUpdateChromeState(UpdateChromeSafetyCheckState state);

// Returns true if `state` is considered an invalid state for the Password
// check in the Safety Check (Magic Stack) module.
bool InvalidPasswordState(PasswordSafetyCheckState state);

// Returns true if `state` is considered an invalid state for the Safe Browsing
// check in the Safety Check (Magic Stack) module.
bool InvalidSafeBrowsingState(SafeBrowsingSafetyCheckState state);

// Returns the number of check issues found given `state`.
int CheckIssuesCount(SafetyCheckState* state);

// Returns true if the Safety Check can be run given `last_run_time`.
bool CanRunSafetyCheck(absl::optional<base::Time> last_run_time);

// Given `last_run_time`, returns a short, human-readable string for the
// timestamp.
NSString* FormatElapsedTimeSinceLastSafetyCheck(
    absl::optional<base::Time> last_run_time);

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_UTILS_H_
