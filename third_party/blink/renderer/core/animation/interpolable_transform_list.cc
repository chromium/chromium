// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_transform_list.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/resolver/transform_builder.h"

namespace blink {

// static
InterpolableTransformList* InterpolableTransformList::ConvertCSSValue(
    const CSSValue& css_value,
    const CSSToLengthConversionData& conversion_data,
    TransformOperations::BoxSizeDependentMatrixBlending box_size_dependent) {
  TransformOperations transform =
      TransformBuilder::CreateTransformOperations(css_value, conversion_data);
  return MakeGarbageCollected<InterpolableTransformList>(std::move(transform),
                                                         box_size_dependent);
}

void InterpolableTransformList::PreConcat(
    const InterpolableTransformList& underlying) {
  HeapVector<Member<TransformOperation>> result;
  result.reserve(underlying.operations_.size() + operations_.size());
  result.AppendVector(underlying.operations_.Operations());
  result.AppendVector(operations_.Operations());
  operations_.Operations() = result;
}

void InterpolableTransformList::AccumulateOnto(
    const InterpolableTransformList& underlying) {
  operations_ = underlying.operations_.Accumulate(operations_);
}

void InterpolableTransformList::Interpolate(const InterpolableValue& to,
                                            const double progress,
                                            InterpolableValue& result) const {
  To<InterpolableTransformList>(result).operations_ =
      To<InterpolableTransformList>(to).operations_.Blend(operations_, progress,
                                                          box_size_dependent_);
}

void InterpolableTransformList::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  // We don't have to check the underlying TransformOperations, as Blend will
  // take care of that and fall-back to discrete animation if needed.
  DCHECK(other.IsTransformList());
}

}  // namespace blink
