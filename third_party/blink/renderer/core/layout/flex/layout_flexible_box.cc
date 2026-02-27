// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/flex/layout_flexible_box.h"

#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_hr_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/flex/flex_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/oof_positioned_node.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

LayoutFlexibleBox::LayoutFlexibleBox(Element* element) : LayoutBlock(element) {}

namespace {

LogicalToPhysical<bool> GetOverflowConverter(const ComputedStyle& style) {
  const bool is_wrap_reverse = style.ResolvedIsFlexWrapReverse();
  const bool is_direction_reverse = style.ResolvedIsReverseFlexDirection();

  bool inline_start = false;
  bool inline_end = true;
  bool block_start = false;
  bool block_end = true;

  if (style.ResolvedIsColumnFlexDirection()) {
    if (is_direction_reverse) {
      std::swap(block_start, block_end);
    }
    if (is_wrap_reverse) {
      std::swap(inline_start, inline_end);
    }
  } else {
    if (is_direction_reverse) {
      std::swap(inline_start, inline_end);
    }
    if (is_wrap_reverse) {
      std::swap(block_start, block_end);
    }
  }

  return LogicalToPhysical(style.GetWritingDirection(), inline_start,
                           inline_end, block_start, block_end);
}

}  // namespace

bool LayoutFlexibleBox::HasTopOverflow() const {
  return GetOverflowConverter(StyleRef()).Top();
}

bool LayoutFlexibleBox::HasLeftOverflow() const {
  return GetOverflowConverter(StyleRef()).Left();
}

// TODO(crbug.com/364348901): We should be able to remove this method entirely
// when the CustomizableSelect flag is removed or disabled, but it causes a
// crash in the switch-picker-appearance WPT.
bool LayoutFlexibleBox::IsChildAllowed(LayoutObject* object,
                                       const ComputedStyle& style) const {
  const auto* select = DynamicTo<HTMLSelectElement>(GetNode());
  // `style` has the wrong appearance value. `select->GetComputedStyle()` is up
  // to date.
  if (select && select->UsesMenuList() &&
      (!select->GetComputedStyle() ||
       !select->SupportsBaseAppearance(
           select->GetComputedStyle()->EffectiveAppearance()))) [[unlikely]] {
    // For a size=1 appearance:auto <select>, we only render the active option
    // label through the InnerElement. We do not allow adding layout objects
    // for options, optgroups, or any other child nodes in order to hide them
    // while still allowing them to have a ComputedStyle.
    return object->GetNode() == &select->InnerElement();
  }
  return LayoutBlock::IsChildAllowed(object, style);
}

void LayoutFlexibleBox::SetNeedsLayoutForDevtools() {
  SetNeedsLayout(layout_invalidation_reason::kDevtools);
  SetNeedsDevtoolsInfo(true);
}

const DevtoolsFlexInfo* LayoutFlexibleBox::FlexLayoutData() const {
  const wtf_size_t fragment_count = PhysicalFragmentCount();
  DCHECK_GE(fragment_count, 1u);
  // Currently, devtools data is on the first fragment of a fragmented flexbox.
  return GetLayoutResult(0)->FlexLayoutData();
}

}  // namespace blink
