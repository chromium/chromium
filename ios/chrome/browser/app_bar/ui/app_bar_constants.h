// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONSTANTS_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import <optional>
#import <string_view>

// State of the AppBar assistant button. These values are logged to UMA. Entries
// should not be renumbered and numeric values should never be reused.
// LINT.IfChange(AppBarAssistantButtonState)
enum class AppBarAssistantButtonState {
  kLens = 0,
  kAsk = 1,
  kAIM = 2,
  kAccount = 3,
  kMaxValue = kAccount,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:IOSAppBarAssistantButtonState)

// Assistant button state chosen by the user in the AppBar assistant button
// menu. It is distinct from `AppBarAssistantButtonState` so that the stored
// values don't depend on it, and so that the absence of choice has its own
// value. It is converted to `AppBarAssistantButtonState` by
// `AssistantButtonStateFromPreferredState()`. These values are persisted to
// `prefs::kAppBarAssistantButtonPreferredState` and logged to UMA. Entries
// should not be renumbered and numeric values should never be reused.
// LINT.IfChange(AppBarAssistantButtonPreferredState)
enum class AppBarAssistantButtonPreferredState {
  // The user didn't choose any state.
  kDefault = 0,
  kLens = 1,
  kAsk = 2,
  kAccount = 3,
  kMaxValue = kAccount,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSAppBarAssistantButtonPreferredState)

// Returns the assistant button state matching `preferred_state`, or
// `std::nullopt` if the user didn't choose any.
std::optional<AppBarAssistantButtonState>
AssistantButtonStateFromPreferredState(
    AppBarAssistantButtonPreferredState preferred_state);

// Returns the height of the app bar in portrait.
CGFloat AppBarHeightPortrait();

// Returns the height of the app bar in portrait, accounting for whether the
// Gemini floaty is invoked and whether the app bar is locked in fullscreen.
CGFloat CurrentAppBarHeightPortrait(BOOL gemini_floaty_invoked,
                                    BOOL app_bar_locked_in_fullscreen);

// The height of the app bar when in fullscreen (portrait).
extern const CGFloat kAppBarHeightFullscreen;

// Returns the height of the app bar in landscape.
CGFloat AppBarHeightLandscape();

// Accessibility identifier for the assistant button.
extern NSString* const kAppBarAssistantButtonId;

// Accessibility identifier for the app bar tab grid button.
extern NSString* const kAppBarTabGridButtonIdentifier;

// Accessibility identifier for the app bar new tab button.
extern NSString* const kAppBarNewTabButtonIdentifier;

// The histogram name for tracking when the assistant button is tapped.
extern const char kAppBarAssistantButtonTappedHistogram[];

// The histogram name for tracking the settled state of the assistant button
// after startup.
extern const char kAppBarAssistantButtonStateOnLoadHistogram[];

// The histogram name for tracking the assistant button state chosen by the
// user, recorded along with `kAppBarAssistantButtonStateOnLoadHistogram`.
inline constexpr std::string_view
    kAppBarAssistantButtonPreferredStateOnLoadHistogram =
        "IOS.AppBar.AssistantButtonPreferredStateOnLoad";

// The histogram name for tracking the state that the assistant button would
// show if the user hadn't chosen any, recorded along with
// `kAppBarAssistantButtonPreferredStateOnLoadHistogram` for users who chose
// one.
inline constexpr std::string_view
    kAppBarAssistantButtonDefaultStateOnLoadHistogram =
        "IOS.AppBar.AssistantButtonDefaultStateOnLoad";

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONSTANTS_H_
