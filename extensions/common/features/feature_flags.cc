// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/feature_flags.h"

#include <algorithm>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "extensions/common/extension_features.h"

namespace extensions {

namespace {

// Feature flags for extension features. These can be used to implement remote
// kill switches for extension features. Note any such feature flags must
// generally be removed once the API has been stable for a few releases.
const base::Feature* kFeatureFlags[] = {
    &extensions_features::kApiContentSettingsClipboard,
    &extensions_features::kApiEnterpriseKioskInput,
    &extensions_features::kApiReadingList,
    &extensions_features::kApiUserScripts,
    &extensions_features::kApiOdfsConfigPrivate,
    &extensions_features::kNewWebstoreDomain,
    &extensions_features::kTelemetryExtensionPendingApprovalApi,
};

CONSTINIT base::span<const base::Feature*> g_feature_flags_test_override;

const base::Feature* GetFeature(const std::string& feature_flag) {
  if (UNLIKELY(!g_feature_flags_test_override.empty())) {
    auto iter = base::ranges::find(g_feature_flags_test_override, feature_flag,
                                   &base::Feature::name);
    return iter == g_feature_flags_test_override.end() ? nullptr : *iter;
  }

  const base::Feature** feature =
      base::ranges::find(kFeatureFlags, feature_flag, &base::Feature::name);

  return feature == std::end(kFeatureFlags) ? nullptr : *feature;
}

}  // namespace

bool IsFeatureFlagEnabled(const std::string& feature_flag) {
  const base::Feature* feature = GetFeature(feature_flag);
  CHECK(feature) << feature_flag;

  // If the feature list is initialized, use its value.
  if (base::FeatureList::GetInstance()) {
    return base::FeatureList::IsEnabled(*feature);
  }

  // There's a chance the feature list isn't initialized. This can happen
  // when the user appended the --pack-extension switch, in which case the
  // pack job happens before the feature list is set up. In this case,
  // use the feature's default state.
  // This isn't a security risk in this case; packing an extension doesn't
  // determine its functionality when installed (and anyone could pack an
  // extension with any feature configuration).
  // TODO(https://crbug.com/1506254): Remove this handling and rely on
  // the FeatureList being initialized.
  return feature->default_state == base::FEATURE_ENABLED_BY_DEFAULT;
}

ScopedFeatureFlagsOverride CreateScopedFeatureFlagsOverrideForTesting(
    base::span<const base::Feature*> features) {
  return base::AutoReset<base::span<const base::Feature*>>(
      &g_feature_flags_test_override, features);
}

}  // namespace extensions
