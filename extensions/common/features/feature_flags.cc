// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/feature_flags.h"

#include <algorithm>
#include <array>

#include "base/check.h"
#include "base/feature_list.h"

namespace extensions {

namespace {

// Feature flags for extension features. These can be used to implement remote
// kill switches for extension features. Note any such feature flags must
// generally be removed once the API has been stable for a few releases.
constexpr std::array<base::Feature, 0> kFeatureFlags{};

const std::vector<base::Feature>* g_feature_flags_test_override = nullptr;

template <typename T>
const base::Feature* GetFeature(T begin,
                                T end,
                                const std::string& feature_flag) {
  T it =
      std::find_if(begin, end, [&feature_flag](const base::Feature& feature) {
        return feature.name == feature_flag;
      });

  return it == end ? nullptr : &(*it);
}

const base::Feature* GetFeature(const std::string& feature_flag) {
  if (g_feature_flags_test_override) {
    return GetFeature(g_feature_flags_test_override->begin(),
                      g_feature_flags_test_override->end(), feature_flag);
  }

  return GetFeature(std::begin(kFeatureFlags), std::end(kFeatureFlags),
                    feature_flag);
}

}  // namespace

bool IsFeatureFlagEnabled(const std::string& feature_flag) {
  const base::Feature* feature = GetFeature(feature_flag);
  CHECK(feature) << feature_flag;
  return base::FeatureList::IsEnabled(*feature);
}

ScopedFeatureFlagsOverride CreateScopedFeatureFlagsOverrideForTesting(
    const std::vector<base::Feature>* features) {
  return base::AutoReset<const std::vector<base::Feature>*>(
      &g_feature_flags_test_override, features);
}

}  // namespace extensions
