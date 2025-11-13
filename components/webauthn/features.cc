// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace webauthn::features {

#if BUILDFLAG(IS_ANDROID)

// Enabled by default in M144. Remove in or after M147.
BASE_FEATURE(kWebAuthnAndroidPasskeyCacheMigration,
             "WebAuthenticationAndroidPasskeyCacheMigration",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Development flag. Not supposed to be enabled by default.
BASE_FEATURE(kWebAuthnAndroidCredManForDev, base::FEATURE_DISABLED_BY_DEFAULT);

// Parameter for `kWebAuthnAndroidCredManForDev` to specify the mode.
// Can be "full" or "parallel".
const base::FeatureParam<std::string> kWebAuthnAndroidCredManForDevMode{
    &kWebAuthnAndroidCredManForDev, "mode", ""};

#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)

// Not yet enabled by default.
BASE_FEATURE(kDeleteOldHiddenPasskeys, base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace webauthn::features
