// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/user_scripts_availability.h"

#include <array>
#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "extensions/common/context_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/user_scripts_allowed_state.h"
#include "url/gurl.h"

namespace extensions::user_scripts_availability {

namespace {

// The set of features which this delegated availability check should apply to.
constexpr static std::array<std::string_view, 4>
    kUserScriptOverrideFeatureList = {
        // LINT.IfChange

        "userScripts",
        // These additional items are necessary because they are listed as
        // individual api features in _api_features.json.
        "userScripts.execute", "userScripts.getWorldConfigurations",
        "userScripts.resetWorldConfiguration",
        // LINT.ThenChange(chrome/common/extensions/extension_test_util.cc)
};

bool AreUserScriptsFeaturesAvailable(
    const std::string& api_full_name,
    const extensions::Extension* extension,
    extensions::mojom::ContextType context,
    const GURL& url,
    extensions::Feature::Platform platform,
    int context_id,
    bool check_developer_mode,
    const extensions::ContextData& context_data) {
  if (!base::FeatureList::IsEnabled(
          extensions_features::kUserScriptUserExtensionToggle)) {
    return true;
  }

  // An extension that no longer exists shouldn't have an API bound for it.
  if (!extension) {
    return false;
  }

  return GetCurrentUserScriptAllowedState(context_id, extension->id())
      .value_or(false);
}

}  // namespace

extensions::Feature::FeatureDelegatedAvailabilityCheckMap
CreateAvailabilityCheckMap() {
  Feature::FeatureDelegatedAvailabilityCheckMap map;
  for (const auto item : kUserScriptOverrideFeatureList) {
    map.emplace(item, base::BindRepeating(&AreUserScriptsFeaturesAvailable));
  }
  return map;
}

}  // namespace extensions::user_scripts_availability
