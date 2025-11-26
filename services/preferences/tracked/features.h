// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_FEATURES_H_
#define SERVICES_PREFERENCES_TRACKED_FEATURES_H_

#include "base/feature_list.h"

namespace tracked {

// Enabled encrypted hashing for prefs.
BASE_DECLARE_FEATURE(kEncryptedPrefHashing);

// Enables encrypted tracked preferences for enterprise users.
BASE_DECLARE_FEATURE(kEnableEncryptedTrackedPrefOnEnterprise);

}  // namespace tracked

#endif  // SERVICES_PREFERENCES_TRACKED_FEATURES_H_
