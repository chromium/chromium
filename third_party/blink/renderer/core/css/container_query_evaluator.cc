// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query_evaluator.h"
#include "third_party/blink/renderer/core/css/container_query.h"

#include "third_party/blink/renderer/core/css/media_values_cached.h"

namespace blink {

namespace {

bool IsSufficientlyContained(PhysicalAxes contained_axes,
                             PhysicalAxes queried_axes) {
  return (contained_axes & queried_axes) == queried_axes;
}

}  // namespace

bool ContainerQueryEvaluator::Eval(
    const ContainerQuery& container_query) const {
  if (container_query.QueriedAxes() == PhysicalAxes(kPhysicalAxisNone))
    return false;
  if (!IsSufficientlyContained(contained_axes_, container_query.QueriedAxes()))
    return false;
  DCHECK(media_query_evaluator_);
  return media_query_evaluator_->Eval(*container_query.media_queries_);
}

void ContainerQueryEvaluator::Add(const ContainerQuery& query, bool result) {
  results_.Set(&query, result);
}

ContainerQueryEvaluator::Change ContainerQueryEvaluator::ContainerChanged(
    PhysicalSize size,
    PhysicalAxes contained_axes) {
  if (size_ == size && contained_axes_ == contained_axes)
    return Change::kNone;

  SetData(size, contained_axes);

  Change change = ComputeChange();

  // We can clear the results here because we will always recaculate the style
  // of all descendants which depend on this evaluator whenever we return
  // something other than kNone from this function, so the results will always
  // be repopulated.
  if (change != Change::kNone)
    results_.clear();

  return change;
}

void ContainerQueryEvaluator::Trace(Visitor* visitor) const {
  visitor->Trace(media_query_evaluator_);
  visitor->Trace(results_);
}

void ContainerQueryEvaluator::SetData(PhysicalSize size,
                                      PhysicalAxes contained_axes) {
  size_ = size;
  contained_axes_ = contained_axes;

  auto* cached_values = MakeGarbageCollected<MediaValuesCached>();
  cached_values->OverrideViewportDimensions(size_.width, size_.height);
  media_query_evaluator_ =
      MakeGarbageCollected<MediaQueryEvaluator>(*cached_values);
}

ContainerQueryEvaluator::Change ContainerQueryEvaluator::ComputeChange() const {
  Change change = Change::kNone;

  for (const auto& result : results_) {
    if (Eval(*result.key) != result.value) {
      change =
          std::max(change, result.key->Name() == g_null_atom ? Change::kUnnamed
                                                             : Change::kNamed);
    }
  }

  return change;
}

}  // namespace blink
