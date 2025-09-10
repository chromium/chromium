// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace webauthn::features {

#if BUILDFLAG(IS_ANDROID)

// Not yet enabled by default.
BASE_FEATURE(kWebAuthnAndroidPasskeyCacheMigration,
             "WebAuthenticationAndroidPasskeyCacheMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)

// Not yet enabled by default.
BASE_FEATURE(kDeleteOldHiddenPasskeys, base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace webauthn::features
