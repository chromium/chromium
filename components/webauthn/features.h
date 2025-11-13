// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_FEATURES_H_
#define COMPONENTS_WEBAUTHN_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace webauthn::features {

#if BUILDFLAG(IS_ANDROID)

// Use the passkey cache service parallel to the FIDO2 module to retrieve
// passkeys from GMSCore. This is for migration.
COMPONENT_EXPORT(WEBAUTHN)
BASE_DECLARE_FEATURE(kWebAuthnAndroidPasskeyCacheMigration);

// A development feature flag to control the CredMan mode.
COMPONENT_EXPORT(WEBAUTHN)
BASE_DECLARE_FEATURE(kWebAuthnAndroidCredManForDev);
// Parameter for `kWebAuthnAndroidCredManForDev` to specify the mode.
// Can be "disabled", "full" or "parallel".
COMPONENT_EXPORT(WEBAUTHN)
extern const base::FeatureParam<std::string> kWebAuthnAndroidCredManForDevMode;

#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)

// Controls deletion of passkeys that have been hidden for a while.
BASE_DECLARE_FEATURE(kDeleteOldHiddenPasskeys);

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace webauthn::features

#endif  // COMPONENTS_WEBAUTHN_FEATURES_H_
