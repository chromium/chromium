// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_AVAILABILITY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_AVAILABILITY_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"

class AuthenticationService;
class PrefService;
class ProfileIOS;

namespace web {
class WebState;
}

namespace gemini {

// Reason why a Gemini entry point is disabled.
enum class EntryPointDisabledReason {
  kQuotaExhausted,
};

// Result describing both the visibility and interactive state of Gemini for an
// entry point.
struct GeminiAvailabilityResult {
  // Whether the Gemini entry point should be visible in the UI.
  bool visible = false;
  // Whether the Gemini entry point should be enabled and interactable (e.g.,
  // tappable). An entry point may be visible but disabled (such as for
  // signed-out users).
  bool enabled = false;
  // The specific reason when the entry point is disabled. Will be
  // `std::nullopt` when the entry point is not disabled or disabled due to
  // general ineligibility.
  std::optional<EntryPointDisabledReason> disabled_reason = std::nullopt;
  // An optional subtitle explaining why the entry point is disabled, if
  // applicable. Will be `nil` when the entry point is enabled or has no
  // disabled subtitle.
  NSString* disabled_reason_subtitle = nil;
  // The specific profile ineligibility reasons when Gemini is not available.
  // Will be `std::nullopt` when the profile is eligible.
  std::optional<IneligibilityReasons> ineligibility_reasons = std::nullopt;
};

// Returns a GeminiAvailabilityResult containing the visibility and enablement
// status of Gemini for the given entry point, profile, and web state. This
// unifies high-level flag checks, profile eligibility, web state availability,
// contextual entry point restrictions, and feature-specific availability into
// a single utility function.
//
// Parameters:
// - `entry_point`: The entry point surface being evaluated. Pass
//   EntryPoint::Unknown to evaluate general Gemini availability for the profile
//   and web state without enforcing entry-point-specific contextual rules.
// - `profile`: Optional user profile to check eligibility for. For bar surfaces
//   (Toolbar, AppBar), if null, will be inferred from `web_state` when
//   available.
// - `web_state`: The WebState for tab-bound entry points (can be nullptr for
//   non-tab-bound surfaces).
// - `auth_service`: Optional AuthenticationService for checking user identity
//   in certain entry points (e.g., Toolbar, AppBar).
// - `pref_service`: Optional PrefService for checking enterprise policy
//   exceptions for signed-out users in certain entry points (e.g., Toolbar,
//   AppBar).
GeminiAvailabilityResult IsGeminiAvailable(
    EntryPoint entry_point,
    ProfileIOS* profile = nullptr,
    web::WebState* web_state = nullptr,
    AuthenticationService* auth_service = nullptr,
    PrefService* pref_service = nullptr);

}  // namespace gemini

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_AVAILABILITY_H_
