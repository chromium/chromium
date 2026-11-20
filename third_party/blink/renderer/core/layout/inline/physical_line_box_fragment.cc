// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/physical_line_box_fragment.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/layout/inline/inline_break_token.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/logical_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/relative_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
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
  base_direction_ = std::to_underlying(builder->base_direction_);
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

PhysicalRect PhysicalLineBoxFragment::ComputeFirstLineBoxRect(
    const ComputedStyle& first_line_style) const {
  if ((IsEmptyLineBox() || metrics_.IsEmpty()) &&
      RuntimeEnabledFeatures::FirstLineBoxDecorationInkOverflowEnabled()) {
    return PhysicalRect();
  }
  const FontHeight text_metrics = first_line_style.GetFontHeight();
  if (IsHorizontalWritingMode(first_line_style.GetWritingMode())) {
    return PhysicalRect(LayoutUnit(), metrics_.ascent - text_metrics.ascent,
                        Size().width, text_metrics.LineHeight());
  }
  return PhysicalRect(Size().width - metrics_.ascent - text_metrics.descent,
                      LayoutUnit(), text_metrics.LineHeight(), Size().height);
}

const ComputedStyle* PhysicalLineBoxFragment::FirstLineBoxDecorationStyle()
    const {
  if (!RuntimeEnabledFeatures::FirstLineBoxDecorationInkOverflowEnabled() ||
      !UsesFirstLineStyle() || IsEmptyLineBox() || metrics_.IsEmpty() ||
      IsHiddenDueToLayout()) {
    return nullptr;
  }
  const LayoutObject* container = ContainerLayoutObject();
  if (!container || container->IsSVGText()) {
    return nullptr;
  }
  const ComputedStyle& style = container->Style();
  const ComputedStyle& first_line_style = container->FirstLineStyle();
  if (&first_line_style == &style ||
      first_line_style.Visibility() != EVisibility::kVisible ||
      (!first_line_style.HasBackground() && !first_line_style.BoxShadow()) ||
      style == first_line_style) {
    return nullptr;
  }
  return &first_line_style;
}

std::optional<PhysicalRect> PhysicalLineBoxFragment::FirstLineBoxInkOverflow(
    const ComputedStyle& first_line_style) const {
  PhysicalRect rect = ComputeFirstLineBoxRect(first_line_style);
  if (first_line_style.HasVisualOverflowingEffect()) {
    rect.Expand(first_line_style.BoxDecorationOutsets());
  }
  if (rect.IsEmpty()) {
    return std::nullopt;
  }
  return rect;
}

void PhysicalLineBoxFragment::TraceAfterDispatch(Visitor* visitor) const {
  PhysicalFragment::TraceAfterDispatch(visitor);
}

}  // namespace blink
