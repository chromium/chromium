// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_trigger_type_impl.h"

namespace content {

bool IsSpeculationRuleType(PrerenderTriggerType type) {
  switch (type) {
    case PrerenderTriggerType::kSpeculationRule:
      [[fallthrough]];
    case PrerenderTriggerType::kSpeculationRuleFromIsolatedWorld:
      return true;
    case PrerenderTriggerType::kEmbedder:
      return false;
  }
}

}  // namespace content
