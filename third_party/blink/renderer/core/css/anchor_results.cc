// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/anchor_results.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

void AnchorItem::Trace(Visitor* visitor) const {
  visitor->Trace(query_);
}

void AnchorResults::Trace(Visitor* visitor) const {
  AnchorEvaluator::Trace(visitor);
  visitor->Trace(map_);
}

std::optional<LayoutUnit> AnchorResults::Evaluate(
    const AnchorQuery& query,
    const ScopedCSSName* position_anchor,
    const std::optional<InsetAreaOffsets>& inset_area_results) {
  // TODO(crbug.com/333423706): Handle `position_anchor`, `inset_area_results`.
  if (GetMode() == AnchorEvaluator::Mode::kNone) {
    return std::nullopt;
  }
  auto* item = MakeGarbageCollected<AnchorItem>(GetMode(), query);
  AnchorResultMap::const_iterator i = map_.find(item);
  if (i != map_.end()) {
    return i->value;
  }
  // Store the missing item explicitly. This causes subsequent calls
  // to IsAnyResultDifferent to check this query as well.
  map_.Set(item, std::nullopt);
  return std::nullopt;
}

std::optional<InsetAreaOffsets> AnchorResults::ComputeInsetAreaOffsetsForLayout(
    const ScopedCSSName* position_anchor,
    InsetArea inset_area) {
  // Only relevant for interleaved anchors.
  return std::nullopt;
}

std::optional<PhysicalOffset> AnchorResults::ComputeAnchorCenterOffsets(
    const ComputedStyleBuilder& builder) {
  // Only relevant for interleaved anchors.
  return std::nullopt;
}

void AnchorResults::Set(AnchorEvaluator::Mode mode,
                        const AnchorQuery& query,
                        std::optional<LayoutUnit> result) {
  map_.Set(MakeGarbageCollected<AnchorItem>(mode, query), result);
}

void AnchorResults::Clear() {
  map_.clear();
}

bool AnchorResults::IsAnyResultDifferent(const ComputedStyle& style,
                                         AnchorEvaluator* evaluator) const {
  ScopedCSSName* position_anchor = style.PositionAnchor();
  for (const auto& [key, old_result] : map_) {
    Mode mode = key->GetMode();
    std::optional<InsetAreaOffsets> inset_area =
        IsBaseMode(mode) ? std::nullopt : style.InsetAreaOffsets();
    AnchorScope anchor_scope(mode, evaluator);
    std::optional<LayoutUnit> new_result =
        evaluator
            ? evaluator->Evaluate(key->Query(), position_anchor, inset_area)
            : std::optional<LayoutUnit>();
    if (new_result != old_result) {
      return true;
    }
  }
  return false;
}

}  // namespace blink
