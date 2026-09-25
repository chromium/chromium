// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIVERSAL_OPTOUT_MODEL_ELIGIBILITY_UTILS_H_
#define IOS_CHROME_BROWSER_UNIVERSAL_OPTOUT_MODEL_ELIGIBILITY_UTILS_H_

class PrefService;

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

}  // namespace universal_optout

#endif  // IOS_CHROME_BROWSER_UNIVERSAL_OPTOUT_MODEL_ELIGIBILITY_UTILS_H_
