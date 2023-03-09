// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/speculation_rules_features.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink::speculation_rules {

bool EagernessEnabled(const FeatureContext* context) {
  return RuntimeEnabledFeatures::SpeculationRulesEagernessEnabled(context) &&
         base::FeatureList::IsEnabled(
             blink::features::kSpeculationRulesEagerness);
}

bool SelectorMatchesEnabled(const FeatureContext* context) {
  return RuntimeEnabledFeatures::
             SpeculationRulesDocumentRulesSelectorMatchesEnabled(context) &&
         base::FeatureList::IsEnabled(
             blink::features::kSpeculationRulesDocumentRulesSelectorMatches);
}

}  // namespace blink::speculation_rules
