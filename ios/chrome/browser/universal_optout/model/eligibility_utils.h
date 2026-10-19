// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIVERSAL_OPTOUT_MODEL_ELIGIBILITY_UTILS_H_
#define IOS_CHROME_BROWSER_UNIVERSAL_OPTOUT_MODEL_ELIGIBILITY_UTILS_H_

class PrefService;
class ProfileIOS;

namespace web {
enum class UniversalOptOutState;
}  // namespace web

namespace universal_optout {

class UniversalOptOutService;

// Returns the eligibility status of the Universal Opt Out feature for the given
// `prefs`. `optout_service` is optional; if null, the eligibility preference
// will be used.
web::UniversalOptOutState GetUniversalOptOutState(
    PrefService* prefs,
    UniversalOptOutService* optout_service = nullptr);

// Returns whether the current OS version supports the GPC extension (between
// iOS 18.4 and iOS 27).
bool IsGPCExtensionSupportedOS();

// Returns whether the Universal Opt-Out extension is enabled for `profile`.
// Returns true only if the current OS is supported (between iOS 18.4 and iOS
// 27), the UniversalOptOutService is available, the feature flags are enabled,
// and extension silent mode is not enabled. User eligibility is not checked
// because eligibility can change dynamically at runtime.
bool IsGPCExtensionEnabled(ProfileIOS* profile);

}  // namespace universal_optout

#endif  // IOS_CHROME_BROWSER_UNIVERSAL_OPTOUT_MODEL_ELIGIBILITY_UTILS_H_
