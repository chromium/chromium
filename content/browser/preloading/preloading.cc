// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading.h"

#include "base/notreached.h"

namespace content {

base::StringPiece PreloadingTypeToString(PreloadingType type) {
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
  NOTREACHED_NORETURN();
}

PreloadingPredictor GetPredictorForSpeculationRules(
    blink::mojom::SpeculationInjectionWorld world) {
  switch (world) {
    case blink::mojom::SpeculationInjectionWorld::kNone:
      [[fallthrough]];
    case blink::mojom::SpeculationInjectionWorld::kMain:
      return content_preloading_predictor::kSpeculationRules;
    case blink::mojom::SpeculationInjectionWorld::kIsolated:
      return content_preloading_predictor::kSpeculationRulesFromIsolatedWorld;
  }
  NOTREACHED_NORETURN();
}

}  // namespace content
