// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/flex/layout_flexible_box.h"

#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/html_hr_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/flex/flex_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/oof_positioned_node.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"

namespace blink {

LayoutFlexibleBox::LayoutFlexibleBox(Element* element) : LayoutBlock(element) {}

namespace {

LogicalToPhysical<bool> GetOverflowConverter(const ComputedStyle& style) {
  const bool is_wrap_reverse = style.FlexWrap() == EFlexWrap::kWrapReverse;
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

bool LayoutFlexibleBox::IsChildAllowed(LayoutObject* object,
                                       const ComputedStyle& style) const {
  const auto* select = DynamicTo<HTMLSelectElement>(GetNode());
  if (select && select->UsesMenuList()) [[unlikely]] {
    if (select->IsAppearanceBaseButton()) {
      CHECK(RuntimeEnabledFeatures::CustomizableSelectEnabled());
      if (IsA<HTMLOptionElement>(object->GetNode()) ||
          IsA<HTMLOptGroupElement>(object->GetNode()) ||
          IsA<HTMLHRElement>(object->GetNode())) {
        // TODO(crbug.com/1511354): Remove this when <option>s are slotted into
        // the UA <datalist>, which will be hidden by default as a popover.
        return false;
      }
      // For appearance:base-select <select>, we want to render all children.
      // However, the InnerElement is only used for rendering in
      // appearance:auto, so don't include that one.
      Node* child = object->GetNode();
      if (child == &select->InnerElement() && select->SlottedButton()) {
        // If the author doesn't provide a button, then we still want to display
        // the InnerElement.
        return false;
      }
      if (auto* popover = select->PopoverForAppearanceBase()) {
        if (child == popover && !popover->popoverOpen()) {
          // This is needed in order to keep the popover hidden after the UA
          // sheet is forcing it to be display:block in order to get a computed
          // style.
          return false;
        }
      }
      return true;
    } else {
      // For a size=1 appearance:auto <select>, we only render the active option
      // label through the InnerElement. We do not allow adding layout objects
      // for options and optgroups.
      return object->GetNode() == &select->InnerElement();
    }
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

void LayoutFlexibleBox::RemoveChild(LayoutObject* child) {
  if (!DocumentBeingDestroyed() &&
      !StyleRef().IsDeprecatedFlexboxUsingFlexLayout())
    MergeAnonymousFlexItems(child);

  LayoutBlock::RemoveChild(child);
}

}  // namespace blink
