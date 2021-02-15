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

ContainerQueryEvaluator::ContainerQueryEvaluator(double width,
                                                 double height,
                                                 PhysicalAxes contained_axes)
    : contained_axes_(contained_axes) {
  auto* cached_values = MakeGarbageCollected<MediaValuesCached>();
  cached_values->OverrideViewportDimensions(width, height);
  media_query_evaluator_ =
      MakeGarbageCollected<MediaQueryEvaluator>(*cached_values);
}

bool ContainerQueryEvaluator::Eval(
    const ContainerQuery& container_query) const {
  if (!IsSufficientlyContained(contained_axes_, container_query.QueriedAxes()))
    return false;
  return media_query_evaluator_->Eval(*container_query.media_queries_);
}

void ContainerQueryEvaluator::Trace(Visitor* visitor) const {
  visitor->Trace(media_query_evaluator_);
}

}  // namespace blink
