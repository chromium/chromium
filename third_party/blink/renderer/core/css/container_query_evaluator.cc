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

ContainerQueryEvaluator::ContainerQueryEvaluator(PhysicalSize size,
                                                 PhysicalAxes contained_axes) {
  SetData(size, contained_axes);
}

bool ContainerQueryEvaluator::Eval(
    const ContainerQuery& container_query) const {
  if (!IsSufficientlyContained(contained_axes_, container_query.QueriedAxes()))
    return false;
  return media_query_evaluator_->Eval(*container_query.media_queries_);
}

bool ContainerQueryEvaluator::ContainerChanged(PhysicalSize size,
                                               PhysicalAxes contained_axes) {
  if (size_ == size && contained_axes_ == contained_axes)
    return false;

  // TODO(crbug.com/1145970): Keep a list of dependent queries, actually
  // evaluate them against the new size, and see if the new size makes an
  // actual difference.
  SetData(size, contained_axes);

  return true;
}

void ContainerQueryEvaluator::Trace(Visitor* visitor) const {
  visitor->Trace(media_query_evaluator_);
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

}  // namespace blink
