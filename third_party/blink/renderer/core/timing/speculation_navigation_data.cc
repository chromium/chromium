// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/speculation_navigation_data.h"

#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-blink.h"

namespace blink {

SpeculationNavigationData::SpeculationNavigationData(
    mojom::blink::SpeculationAction action,
    const KURL& url,
    const std::optional<Vector<String>>& tags,
    std::optional<mojom::blink::SpeculationEagerness> eagerness)
    : action_(action), url_(url), tags_(tags), eagerness_(eagerness) {}

V8SpeculationNavigationType SpeculationNavigationData::type() const {
  switch (action_) {
    case mojom::blink::SpeculationAction::kPrefetch:
      return V8SpeculationNavigationType(
          V8SpeculationNavigationType::Enum::kPrefetch);
    case mojom::blink::SpeculationAction::kPrerender:
      return V8SpeculationNavigationType(
          V8SpeculationNavigationType::Enum::kPrerender);
    case mojom::blink::SpeculationAction::kPrerenderUntilScript:
      return V8SpeculationNavigationType(
          V8SpeculationNavigationType::Enum::kPrerenderUntilScript);
  }
}

std::optional<V8SpeculationEagernessValue>
SpeculationNavigationData::eagerness() const {
  if (!eagerness_.has_value()) {
    return std::nullopt;
  }

  switch (eagerness_.value()) {
    case mojom::blink::SpeculationEagerness::kConservative:
      return V8SpeculationEagernessValue(
          V8SpeculationEagernessValue::Enum::kConservative);
    case mojom::blink::SpeculationEagerness::kModerate:
      return V8SpeculationEagernessValue(
          V8SpeculationEagernessValue::Enum::kModerate);
    case mojom::blink::SpeculationEagerness::kImmediate:
      return V8SpeculationEagernessValue(
          V8SpeculationEagernessValue::Enum::kImmediate);
    case mojom::blink::SpeculationEagerness::kEager:
      return V8SpeculationEagernessValue(
          V8SpeculationEagernessValue::Enum::kEager);
  }
}

void SpeculationNavigationData::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
