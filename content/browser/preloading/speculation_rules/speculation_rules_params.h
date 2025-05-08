// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_SPECULATION_RULES_SPECULATION_RULES_PARAMS_H_
#define CONTENT_BROWSER_PRELOADING_SPECULATION_RULES_SPECULATION_RULES_PARAMS_H_

#include <optional>

#include "content/browser/preloading/speculation_rules/speculation_rules_tags.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-shared.h"

namespace content {

// This struct holds parameters specific to the Speculation Rules trigger.
struct CONTENT_EXPORT SpeculationRulesParams {
  SpeculationRulesParams();
  SpeculationRulesParams(blink::mojom::SpeculationTargetHint target_hint,
                         blink::mojom::SpeculationEagerness eagerness,
                         SpeculationRulesTags tags);
  ~SpeculationRulesParams();

  // Copyable and movable.
  SpeculationRulesParams(const SpeculationRulesParams&);
  SpeculationRulesParams& operator=(const SpeculationRulesParams&);
  SpeculationRulesParams(SpeculationRulesParams&&) noexcept;
  SpeculationRulesParams& operator=(SpeculationRulesParams&&) noexcept;

  // https://wicg.github.io/nav-speculation/speculation-rules.html#speculation-rule-target-navigable-name-hint
  blink::mojom::SpeculationTargetHint target_hint =
      blink::mojom::SpeculationTargetHint::kNoHint;

  // https://wicg.github.io/nav-speculation/speculation-rules.html#speculation-rule-eagerness
  blink::mojom::SpeculationEagerness eagerness =
      blink::mojom::SpeculationEagerness::kConservative;

  // https://wicg.github.io/nav-speculation/speculation-rules.html#speculation-rule-tags
  SpeculationRulesTags tags;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_SPECULATION_RULES_SPECULATION_RULES_PARAMS_H_
