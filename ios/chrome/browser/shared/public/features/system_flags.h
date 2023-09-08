// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_FEATURES_SYSTEM_FLAGS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_FEATURES_SYSTEM_FLAGS_H_

#import <string>

#import "third_party/abseil-cpp/absl/types/optional.h"

#import <Foundation/Foundation.h>

enum class UpdateChromeSafetyCheckState;
enum class PasswordSafetyCheckState;
enum class SafeBrowsingSafetyCheckState;

// This file can be empty. Its purpose is to contain the flags living in the
// System Settings, used for testing/debugging. No base::Feature (or check for
// them) should be added.

namespace experimental_flags {

// Whether the First Run UI will be always be displayed.
bool AlwaysDisplayFirstRun();

// Whether the Upgrade Promo UI will be always be displayed.
bool AlwaysDisplayUpgradePromo();

// Returns the host name for an alternative Origin Server host for use by
// `BrandCode` startup ping. Returns empty string if there is no alternative
// host specified.
NSString* GetOriginServerHost();

// Returns the URL for the alternative Discover Feed server.
NSString* GetAlternateDiscoverFeedServerURL();

// Returns true if the prefs for the notice card views count and clicks count
// should be reset to zero on feed start.
// TODO(crbug.com/1189232): Remove after launch.
bool ShouldResetNoticeCardOnFeedStart();

// Returns true if the count of showing the First Follow modal should be reset
// to zero.
// TODO(crbug.com/1312124): Remove after launch.
bool ShouldResetFirstFollowCount();

// Returns true if the top of feed signin promo should be shown regardless of
// dismissal conditions. The promo will still only show for signed out users.
bool ShouldForceFeedSigninPromo();

// Returns true if Tile Ablation should be forced regardless of the value of
// `isTileAblationExperimentComplete`.
bool ShouldIgnoreTileAblationConditions();

// Should be called after the count has been reset so that the resetting flag
// can be turned off.
// TODO(crbug.com/1312124): Remove after launch.
void DidResetFirstFollowCount();

// Returns true if the First Follow modal should always be shown when the user
// follows a channel.
// TODO(crbug.com/1312124): Remove after launch.
bool ShouldAlwaysShowFirstFollow();

// Returns true if the Follow IPH should always be shown when the user
// browsing a eligible website in non-incognito mode.
// TODO(crbug.com/1340154): Remove after launch.
bool ShouldAlwaysShowFollowIPH();

// Whether memory debugging tools are enabled.
bool IsMemoryDebuggingEnabled();

// Whether omnibox debugging tools are enabled.
bool IsOmniboxDebuggingEnabled();

// Whether Spotlight debugging tools are enabled.
bool IsSpotlightDebuggingEnabled();

// Whether startup crash is enabled.
bool IsStartupCrashEnabled();

// Whether the 3rd party keyboard omnibox workaround is enabled.
bool IsThirdPartyKeyboardWorkaroundEnabled();

// Whether the application group sandbox must be cleared before starting.
// Calling this method will reset the flag to false, so the sandbox is cleared
// only once.
bool MustClearApplicationGroupSandbox();

// Returns the name of the promo to be forced to display when the app is
// launched or resumed. Returns empty string if no promo is to be forced
// to display. Always returns nil for users in stable/beta.
NSString* GetForcedPromoToDisplay();

// Returns the forced state of the Update Chrome check in the Safety Check
// (Magic Stack) module.
absl::optional<UpdateChromeSafetyCheckState> GetUpdateChromeSafetyCheckState();

// Returns the forced state of the Password check in the Safety Check (Magic
// Stack) module.
absl::optional<PasswordSafetyCheckState> GetPasswordSafetyCheckState();

// Returns the forced state of the Safe Browsing check in the Safety Check
// (Magic Stack) module.
absl::optional<SafeBrowsingSafetyCheckState> GetSafeBrowsingSafetyCheckState();

// Returns the forced number of weak passwords for the Safety Check (Magic
// Stack) module.
absl::optional<int> GetSafetyCheckWeakPasswordsCount();

// Returns the forced number of reused passwords for the Safety Check (Magic
// Stack) module.
absl::optional<int> GetSafetyCheckReusedPasswordsCount();

// Returns the forced number of compromised passwords for the Safety Check
// (Magic Stack) module.
absl::optional<int> GetSafetyCheckCompromisedPasswordsCount();

// Returns the selected device segment the user wants to simulate as a string;
// the string should either be nil or one of the options from synthetic trial
// "Segmentation_DeviceSwitcher."
// The value could be set both from Experimental Settings and command line
// switches, but the former takes precedence.
std::string GetSegmentForForcedDeviceSwitcherExperience();

// Whether a phone backup/restore state should be simulated.
bool SimulatePostDeviceRestore();

}  // namespace experimental_flags

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_FEATURES_SYSTEM_FLAGS_H_
