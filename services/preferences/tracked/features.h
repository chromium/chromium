// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_FEATURES_H_
#define SERVICES_PREFERENCES_TRACKED_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace tracked {

// Enabled encrypted hashing for prefs.
BASE_DECLARE_FEATURE(kEncryptedPrefHashing);

// Enables encrypted tracked preferences for enterprise users.
BASE_DECLARE_FEATURE(kEnableEncryptedTrackedPrefOnEnterprise);

#if BUILDFLAG(IS_WIN)
// Reject weak ciphertext if a stronger algorithm is available.
BASE_DECLARE_FEATURE(kRejectWeakCiphertext);
#endif  // BUILDFLAG(IS_WIN)

// When enabled, disables the legacy HMAC fallback upgrade path during
// validation with os_crypt. Missing encrypted hashes will not be healed via
// legacy MACs, preventing downgrade attacks.
BASE_DECLARE_FEATURE(kDisallowLegacyPrefMacFallback);

// When enabled, an encrypted hash that can not be decrypted because its key is
// unavailable or has changed (e.g. the OS key was recreated) is treated as if
// it were missing, rather than as a changed value. This allows the legacy HMAC
// fallback (if not disallowed by kDisallowLegacyPrefMacFallback) to validate
// the value and re-stamp the encrypted hash with the current key, instead of
// resetting the preference. Kill switch for https://crbug.com/575543754.
BASE_DECLARE_FEATURE(kTreatUndecryptableEncryptedHashAsMissing);

}  // namespace tracked

#endif  // SERVICES_PREFERENCES_TRACKED_FEATURES_H_
