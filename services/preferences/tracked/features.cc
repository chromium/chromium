// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "services/preferences/tracked/features.h"

#include "base/feature_list.h"

namespace tracked {

// Enables hashing of encrypted pref values for integrity checks.
BASE_FEATURE(kEncryptedPrefHashing,
             "EncryptedPrefHashing",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace tracked
