// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_MDM_ALLOWLISTED_SCOPES_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_MDM_ALLOWLISTED_SCOPES_H_

#include <set>
#include <string>

#include "google_apis/gaia/gaia_constants.h"

namespace signin {

inline const std::set<std::string> kMdmAllowlistedScopes{
    GaiaConstants::kGoogleUserInfoEmail,
    GaiaConstants::kGoogleUserInfoProfile,
    GaiaConstants::kChromeSyncOAuth2Scope,
    GaiaConstants::kOAuth1LoginScope,
};

}  // namespace signin

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_MDM_ALLOWLISTED_SCOPES_H_
