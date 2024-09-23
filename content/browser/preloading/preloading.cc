// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading.h"

#include <string_view>

#include "base/notreached.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/preloading_trigger_type.h"

namespace content {

std::string_view PreloadingTypeToString(PreloadingType type) {
  switch (type) {
    case PreloadingType::kUnspecified:
      return "Unspecified";
    case PreloadingType::kPreconnect:
      return "Preconnect";
    case PreloadingType::kPrefetch:
      return "Prefetch";
    case PreloadingType::kPrerender:
      return "Prerender";
    case PreloadingType::kNoStatePrefetch:
      return "NoStatePrefetch";
    case PreloadingType::kLinkPreview:
      return "LinkPreview";
  }
  NOTREACHED();
}

PreloadingPredictor GetPredictorForPreloadingTriggerType(
    PreloadingTriggerType type) {
  switch (type) {
    case PreloadingTriggerType::kSpeculationRule:
      return content_preloading_predictor::kSpeculationRules;
    case PreloadingTriggerType::kSpeculationRuleFromIsolatedWorld:
      return content_preloading_predictor::kSpeculationRulesFromIsolatedWorld;
    case PreloadingTriggerType::kSpeculationRuleFromAutoSpeculationRules:
      return content_preloading_predictor::
          kSpeculationRulesFromAutoSpeculationRules;
    case PreloadingTriggerType::kEmbedder:
      // GetPredictorForPreloadingTriggerType is currently called for
      // speculation rules code-path only, thus NOTREACHED().
      // However there is nothing fundamentally wrong with calling it
      // for embedder trigger code-path (while you might want to be specific
      // about the `PreloadingPredictor` more than just "embedder").
      // Revisit if needed.
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED();
}

}  // namespace content
