// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_FEATURE_FLAGS_H_
#define EXTENSIONS_COMMON_FEATURES_FEATURE_FLAGS_H_

#include <string>

#include "base/auto_reset.h"
#include "base/containers/span.h"
#include "base/feature_list.h"

namespace extensions {

// Returns true if the |feature_flag| with the given name is enabled. This
// CHECKs to validate that |feature_flag| corresponds to a base::Feature of the
// same name.
bool IsFeatureFlagEnabled(const std::string& feature_flag);

// Used to override the set of base::Feature flags while the returned scoper is
// alive. Clients must ensure that pointers in |features| remain valid
// (non-dangling) while the returned scoper is alive (note that features are
// generally global variables, so this should always be trivially true...).
using ScopedFeatureFlagsOverride =
    base::AutoReset<base::span<const base::Feature*>>;
ScopedFeatureFlagsOverride CreateScopedFeatureFlagsOverrideForTesting(
    base::span<const base::Feature*> features);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_FEATURE_FLAGS_H_
