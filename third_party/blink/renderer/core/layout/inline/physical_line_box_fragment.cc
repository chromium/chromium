// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/physical_line_box_fragment.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/layout/inline/inline_break_token.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/logical_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/relative_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

namespace {

struct SameSizeAsPhysicalLineBoxFragment : PhysicalFragment {
  FontHeight metrics;
};

ASSERT_SIZE(PhysicalLineBoxFragment, SameSizeAsPhysicalLineBoxFragment);

}  // namespace

const PhysicalLineBoxFragment* PhysicalLineBoxFragment::Create(
    LineBoxFragmentBuilder* builder) {
  DCHECK_EQ(builder->children_.size(), 0u);
  return MakeGarbageCollected<PhysicalLineBoxFragment>(PassKey(), builder);
}

const PhysicalLineBoxFragment* PhysicalLineBoxFragment::Clone(
    const PhysicalLineBoxFragment& other) {
  return MakeGarbageCollected<PhysicalLineBoxFragment>(PassKey(), other);
}

PhysicalLineBoxFragment::PhysicalLineBoxFragment(
    PassKey key,
    LineBoxFragmentBuilder* builder)
    : PhysicalFragment(builder,
                       builder->GetWritingMode(),
                       kFragmentLineBox,
                       builder->line_box_type_),
      metrics_(builder->metrics_) {
  // A line box must have a metrics unless it's an empty line box.
  DCHECK(!metrics_.IsEmpty() || IsEmptyLineBox());
  base_direction_ = static_cast<unsigned>(builder->base_direction_);
  has_hanging_ = builder->hang_inline_size_ != 0;
  has_propagated_descendants_ = has_floating_descendants_for_paint_ ||
                                HasOutOfFlowPositionedDescendants() ||
                                builder->unpositioned_list_marker_;
}

PhysicalLineBoxFragment::PhysicalLineBoxFragment(
    PassKey key,
    const PhysicalLineBoxFragment& other)
    : PhysicalFragment(other), metrics_(other.metrics_) {
  base_direction_ = other.base_direction_;
  has_hanging_ = other.has_hanging_;
  has_propagated_descendants_ = other.has_propagated_descendants_;
}

PhysicalLineBoxFragment::~PhysicalLineBoxFragment() = default;

FontHeight PhysicalLineBoxFragment::BaselineMetrics() const {
  // TODO(kojii): Computing other baseline types than the used one is not
  // implemented yet.
  // TODO(kojii): We might need locale/script to look up OpenType BASE table.
  return metrics_;
}

bool PhysicalLineBoxFragment::HasSoftWrapToNextLine() const {
  const auto* break_token = To<InlineBreakToken>(GetBreakToken());
  return break_token && !break_token->IsForcedBreak();
}

void PhysicalLineBoxFragment::TraceAfterDispatch(Visitor* visitor) const {
  PhysicalFragment::TraceAfterDispatch(visitor);
}

}  // namespace blink
