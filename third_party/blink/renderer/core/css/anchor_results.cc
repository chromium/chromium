// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/anchor_results.h"

#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

void AnchorItem::Trace(Visitor* visitor) const {
  visitor->Trace(query_);
}

void AnchorResults::Trace(Visitor* visitor) const {
  visitor->Trace(map_);
}

std::optional<LayoutUnit> AnchorResults::Evaluate(const AnchorQuery& query) {
  if (GetMode() == AnchorScope::Mode::kNone) {
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

void AnchorResults::Set(AnchorScope::Mode mode,
                        const AnchorQuery& query,
                        std::optional<LayoutUnit> result) {
  map_.Set(MakeGarbageCollected<AnchorItem>(mode, query), result);
}

void AnchorResults::Clear() {
  map_.clear();
}

bool AnchorResults::IsAnyResultDifferent(AnchorEvaluator* evaluator) const {
  for (const auto& [key, old_result] : map_) {
    AnchorScope anchor_scope(key->GetMode(), evaluator);
    std::optional<LayoutUnit> new_result =
        evaluator ? evaluator->Evaluate(key->Query())
                  : std::optional<LayoutUnit>();
    if (new_result != old_result) {
      return true;
    }
  }
  return false;
}

}  // namespace blink
