// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_FEATURES_H_
#define COMPONENTS_WEBAUTHN_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"

namespace webauthn::features {

#if BUILDFLAG(IS_ANDROID)

// Use the passkey cache service parallel to the FIDO2 module to retrieve
// passkeys from GMSCore. This is for migration.
// The feature is still exposed to Java via DeviceFeatureMap, so it needs to be
// exported.
COMPONENT_EXPORT(WEBAUTHN)
BASE_DECLARE_FEATURE(kWebAuthnAndroidPasskeyCacheMigration);

#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)

// Controls deletion of passkeys that have been hidden for a while.
BASE_DECLARE_FEATURE(kDeleteOldHiddenPasskeys);

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace webauthn::features

#endif  // COMPONENTS_WEBAUTHN_FEATURES_H_
