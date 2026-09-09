// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/mime_handler_availability.h"

#include <optional>
#include <string_view>

#include "base/feature_list.h"
#include "extensions/common/context_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "url/gurl.h"

namespace extensions::mime_handler_availability {

namespace {

bool IsMimeHandlerApiAvailable(
    std::string_view /*api_full_name*/,
    const extensions::Extension* /*extension*/,
    extensions::mojom::ContextType /*context*/,
    const GURL& /*url*/,
    extensions::Feature::Platform /*platform*/,
    int /*context_id*/,
    bool /*check_developer_mode*/,
    const extensions::ContextData& /*context_data*/) {
  // _api_features.json governs where the API is available by default; an
  // explicit override (chrome://flags or --disable-features) must still
  // win, so consult only the override state, not the flag's default.
  const std::optional<bool> override_state =
      base::FeatureList::GetStateIfOverridden(
          extensions_features::kApiMimeHandler);
  return !override_state.has_value() || *override_state;
}

}  // namespace

Feature::FeatureDelegatedAvailabilityCheckMap CreateAvailabilityCheckMap() {
  Feature::FeatureDelegatedAvailabilityCheckMap map;
  map.emplace("mimeHandler", &IsMimeHandlerApiAvailable);
  return map;
}

}  // namespace extensions::mime_handler_availability
