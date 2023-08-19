// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/flex/layout_ng_flexible_box.h"

#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_positioned_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

LayoutNGFlexibleBox::LayoutNGFlexibleBox(Element* element)
    : LayoutNGMixin<LayoutBlock>(element) {}

bool LayoutNGFlexibleBox::HasTopOverflow() const {
  const auto& style = StyleRef();
  bool is_wrap_reverse = StyleRef().FlexWrap() == EFlexWrap::kWrapReverse;
  if (style.IsHorizontalWritingMode()) {
    return style.ResolvedIsColumnReverseFlexDirection() ||
           (style.ResolvedIsRowFlexDirection() && is_wrap_reverse);
  }
  return style.IsLeftToRightDirection() ==
         (style.ResolvedIsRowReverseFlexDirection() ||
          (style.ResolvedIsColumnFlexDirection() && is_wrap_reverse));
}

bool LayoutNGFlexibleBox::HasLeftOverflow() const {
  const auto& style = StyleRef();
  bool is_wrap_reverse = StyleRef().FlexWrap() == EFlexWrap::kWrapReverse;
  if (style.IsHorizontalWritingMode()) {
    return style.IsLeftToRightDirection() ==
           (style.ResolvedIsRowReverseFlexDirection() ||
            (style.ResolvedIsColumnFlexDirection() && is_wrap_reverse));
  }
  return (style.GetWritingMode() == WritingMode::kVerticalLr) ==
         (style.ResolvedIsColumnReverseFlexDirection() ||
          (style.ResolvedIsRowFlexDirection() && is_wrap_reverse));
}

namespace {

void MergeAnonymousFlexItems(LayoutObject* remove_child) {
  // When we remove a flex item, and the previous and next siblings of the item
  // are text nodes wrapped in anonymous flex items, the adjacent text nodes
  // need to be merged into the same flex item.
  LayoutObject* prev = remove_child->PreviousSibling();
  if (!prev || !prev->IsAnonymousBlock())
    return;
  LayoutObject* next = remove_child->NextSibling();
  if (!next || !next->IsAnonymousBlock())
    return;
  To<LayoutBoxModelObject>(next)->MoveAllChildrenTo(
      To<LayoutBoxModelObject>(prev));
  next->Destroy();
}

}  // namespace

// See LayoutFlexibleBox::IsChildAllowed().
bool LayoutNGFlexibleBox::IsChildAllowed(LayoutObject* object,
                                         const ComputedStyle& style) const {
  const auto* select = DynamicTo<HTMLSelectElement>(GetNode());
  if (UNLIKELY(select && select->UsesMenuList())) {
    // For a size=1 <select>, we only render the active option label through the
    // InnerElement. We do not allow adding layout objects for options and
    // optgroups.
    return object->GetNode() == &select->InnerElement();
  }
  return LayoutNGMixin<LayoutBlock>::IsChildAllowed(object, style);
}

void LayoutNGFlexibleBox::SetNeedsLayoutForDevtools() {
  SetNeedsLayout(layout_invalidation_reason::kDevtools);
  SetNeedsDevtoolsInfo(true);
}

const DevtoolsFlexInfo* LayoutNGFlexibleBox::FlexLayoutData() const {
  const wtf_size_t fragment_count = PhysicalFragmentCount();
  DCHECK_GE(fragment_count, 1u);
  // Currently, devtools data is on the first fragment of a fragmented flexbox.
  return GetLayoutResult(0)->FlexLayoutData();
}

void LayoutNGFlexibleBox::RemoveChild(LayoutObject* child) {
  if (!DocumentBeingDestroyed() &&
      !StyleRef().IsDeprecatedFlexboxUsingFlexLayout())
    MergeAnonymousFlexItems(child);

  LayoutBlock::RemoveChild(child);
}

}  // namespace blink
