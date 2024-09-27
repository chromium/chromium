// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_FEATURES_SYSTEM_FLAGS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_FEATURES_SYSTEM_FLAGS_H_

#import <Foundation/Foundation.h>

#import <optional>
#import <string>

enum class UpdateChromeSafetyCheckState;
enum class PasswordSafetyCheckState;
enum class SafeBrowsingSafetyCheckState;

// This file can be empty. Its purpose is to contain the flags living in the
// System Settings, used for testing/debugging. No base::Feature (or check for
// them) should be added.

namespace experimental_flags {

// NSUserDefaults key to list the number of profile available.
extern NSString* const kDisplaySwitchProfile;

// Whether the First Run UI will always be displayed.
bool AlwaysDisplayFirstRun();

// Whether the Upgrade Promo UI will always be displayed.
bool AlwaysDisplayUpgradePromo();

// Returns the host name for an alternative Origin Server host for use by
// `BrandCode` startup ping. Returns empty string if there is no alternative
// host specified.
NSString* GetOriginServerHost();

// Returns the URL for the alternative Discover Feed server.
NSString* GetAlternateDiscoverFeedServerURL();

// Returns true if the prefs for the notice card views count and clicks count
// should be reset to zero on feed start.
// TODO(crbug.com/40173621): Remove after launch.
bool ShouldResetNoticeCardOnFeedStart();

// Returns true if the count of showing the First Follow modal should be reset
// to zero.
// TODO(crbug.com/40220465): Remove after launch.
bool ShouldResetFirstFollowCount();

// Returns true if the top of feed signin promo should be shown regardless of
// dismissal conditions. The promo will still only show for signed out users.
bool ShouldForceFeedSigninPromo();

// Returns true if the top of feed notifications promo should be shown
// regardless of dismissal conditions. It is only shown for signed in users.
bool ShouldForceContentNotificationsPromo();

// Returns true if Tile Ablation should be forced regardless of the value of
// `isTileAblationExperimentComplete`.
bool ShouldIgnoreTileAblationConditions();

// Should be called after the count has been reset so that the resetting flag
// can be turned off.
// TODO(crbug.com/40220465): Remove after launch.
void DidResetFirstFollowCount();

// Returns true if the First Follow modal should always be shown when the user
// follows a channel.
// TODO(crbug.com/40220465): Remove after launch.
bool ShouldAlwaysShowFirstFollow();

// Returns true if the Follow IPH should always be shown when the user
// browsing a eligible website in non-incognito mode.
// TODO(crbug.com/40230248): Remove after launch.
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
std::optional<UpdateChromeSafetyCheckState> GetUpdateChromeSafetyCheckState();

// Returns the forced state of the Password check in the Safety Check (Magic
// Stack) module.
std::optional<PasswordSafetyCheckState> GetPasswordSafetyCheckState();

// Returns the forced state of the Safe Browsing check in the Safety Check
// (Magic Stack) module.
std::optional<SafeBrowsingSafetyCheckState> GetSafeBrowsingSafetyCheckState();

// Returns the forced number of weak passwords for the Safety Check (Magic
// Stack) module.
std::optional<int> GetSafetyCheckWeakPasswordsCount();

// Returns the forced number of reused passwords for the Safety Check (Magic
// Stack) module.
std::optional<int> GetSafetyCheckReusedPasswordsCount();

// Returns the forced number of compromised passwords for the Safety Check
// (Magic Stack) module.
std::optional<int> GetSafetyCheckCompromisedPasswordsCount();

// Returns the forced number of days since first run.
std::optional<int> GetFirstRunRecency();

// Returns the selected device segment the user wants to simulate as a string;
// the string should either be nil or one of the options from synthetic trial
// "Segmentation_DeviceSwitcher."
// The value could be set both from Experimental Settings and command line
// switches, but the former takes precedence.
std::string GetSegmentForForcedDeviceSwitcherExperience();

// Returns the selected shopper segment the user wants to simulate as a string.
// The string should either be nil, "ShoppingUser", or "Other". The value could
// be set both from Experimental Settings and command line switches, but the
// former takes precedence.
std::string GetSegmentForForcedShopperExperience();

// Whether a phone backup/restore state should be simulated.
bool SimulatePostDeviceRestore();

// In production, the history sync opt-in isn't shown if it was declined too
// recently or too many consecutive times. If this function is true, those
// limits are suppressed for simpler testing.
bool ShouldIgnoreHistorySyncDeclineLimits();

// Whether the developer-mode Switch Profile UI will be be displayed, returns
// the number of test profiles that should be created.
std::optional<int> DisplaySwitchProfile();

// Returns the inactivity threshold to be used for displaying Safety Check
// notifications, overriding the default value stored in the code or any value
// set by Finch.
//
// Returns `std::nullopt` if no override is specified.
std::optional<int> GetForcedInactivityThresholdForSafetyCheckNotifications();

// Returns the forced state of the Tips (Magic Stack) module.
std::optional<int> GetForcedTipsMagicStackState();

// Whether the Lens Shop state for Tips (Magic Stack) should display a product
// image.
bool ShouldDisplayLensShopTipWithImage();

// Returns the override for Tab Resumption decoration.
// Returns nil is not set.
NSString* GetTabResumptionDecorationOverride();

}  // namespace experimental_flags

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_FEATURES_SYSTEM_FLAGS_H_
