// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_trigger_type_impl.h"

namespace content {

bool IsSpeculationRuleType(PreloadingTriggerType type) {
  switch (type) {
    case PreloadingTriggerType::kSpeculationRule:
    case PreloadingTriggerType::kSpeculationRuleFromIsolatedWorld:
    case PreloadingTriggerType::kSpeculationRuleFromAutoSpeculationRules:
      return true;
    case PreloadingTriggerType::kEmbedder:
      return false;
  }
}

}  // namespace content
