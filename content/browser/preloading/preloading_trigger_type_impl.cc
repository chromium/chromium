// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_trigger_type_impl.h"

namespace content {

PreloadingTriggerType PreloadingTriggerTypeFromSpeculationInjectionType(
    blink::mojom::SpeculationInjectionType injection_type) {
  switch (injection_type) {
    case blink::mojom::SpeculationInjectionType::kNone:
      [[fallthrough]];
    case blink::mojom::SpeculationInjectionType::kMainWorldScript:
      return PreloadingTriggerType::kSpeculationRule;
    case blink::mojom::SpeculationInjectionType::kIsolatedWorldScript:
      return PreloadingTriggerType::kSpeculationRuleFromIsolatedWorld;
    case blink::mojom::SpeculationInjectionType::kAutoSpeculationRules:
      return PreloadingTriggerType::kSpeculationRuleFromAutoSpeculationRules;
  }
}

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
