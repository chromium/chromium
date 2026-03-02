// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "services/preferences/tracked/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace tracked {

// Enables hashing of encrypted pref values for integrity checks.
BASE_FEATURE(kEncryptedPrefHashing, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables encrypted tracked preferences for enterprise users.
BASE_FEATURE(kEnableEncryptedTrackedPrefOnEnterprise,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// Reject weak ciphertext if a stronger algorithm is available.
BASE_FEATURE(kRejectWeakCiphertext, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace tracked
