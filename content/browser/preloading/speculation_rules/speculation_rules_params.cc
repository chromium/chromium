// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/speculation_rules/speculation_rules_params.h"

namespace content {

SpeculationRulesParams::SpeculationRulesParams() = default;
SpeculationRulesParams::~SpeculationRulesParams() = default;

SpeculationRulesParams::SpeculationRulesParams(
    blink::mojom::SpeculationTargetHint target_hint,
    blink::mojom::SpeculationEagerness eagerness,
    SpeculationRulesTags tags)
    : target_hint(target_hint), eagerness(eagerness), tags(std::move(tags)) {}

SpeculationRulesParams::SpeculationRulesParams(const SpeculationRulesParams&) =
    default;
SpeculationRulesParams& SpeculationRulesParams::operator=(
    const SpeculationRulesParams&) = default;
SpeculationRulesParams::SpeculationRulesParams(
    SpeculationRulesParams&&) noexcept = default;
SpeculationRulesParams& SpeculationRulesParams::operator=(
    SpeculationRulesParams&&) noexcept = default;

}  // namespace content
