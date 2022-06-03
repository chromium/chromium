// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_FEATURE_FLAGS_H_
#define EXTENSIONS_COMMON_FEATURES_FEATURE_FLAGS_H_

#include <string>
#include <vector>

#include "base/auto_reset.h"

namespace base {
struct Feature;
}  // namespace base

namespace extensions {

// Returns true if the |feature_flag| with the given name is enabled. This
// CHECKs to validate that |feature_flag| corresponds to a base::Feature of the
// same name.
bool IsFeatureFlagEnabled(const std::string& feature_flag);

// Used to override the set of base::Feature flags while the returned value is
// in scope. Clients must ensure that |features| remains alive (non-dangling)
// while the returned value is in scope.
using ScopedFeatureFlagsOverride =
    base::AutoReset<const std::vector<base::Feature>*>;
ScopedFeatureFlagsOverride CreateScopedFeatureFlagsOverrideForTesting(
    const std::vector<base::Feature>* features);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_FEATURE_FLAGS_H_
