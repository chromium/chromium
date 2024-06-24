// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_scroll_container_with_markers.h"

#include "third_party/blink/renderer/core/css/properties/longhands.h"

namespace blink {

LayoutScrollContainerWithMarkers::LayoutScrollContainerWithMarkers(
    Element* element)
    : LayoutBlockFlow(element) {
  SetChildrenInline(false);
}

void LayoutScrollContainerWithMarkers::AddChild(LayoutObject* new_child,
                                                LayoutObject* before_child) {
  LayoutObject* scroller = FindAnonymousContentBox();
  if (new_child->IsScrollMarkerGroup()) {
    CHECK(!FindScrollMarkerGroup());
    LayoutBlockFlow::AddChild(
        new_child, new_child->IsScrollMarkerGroupBefore() ? scroller : nullptr);
    return;
  }
  CHECK(scroller);
  scroller->AddChild(new_child, before_child);
}

void LayoutScrollContainerWithMarkers::InsertedIntoTree() {
  LayoutBlockFlow::InsertedIntoTree();

  if (FindAnonymousContentBox()) {
    return;
  }

  // TODO(crbug.com/332396355): Consider other display types not mentioned in
  // the spec (ex. EDisplay::kLayoutCustom).
  EDisplay display = EDisplay::kFlowRoot;
  switch (StyleRef().Display()) {
    case EDisplay::kFlex:
    case EDisplay::kInlineFlex:
      display = EDisplay::kFlex;
      break;
    case EDisplay::kGrid:
    case EDisplay::kInlineGrid:
      display = EDisplay::kGrid;
      break;
    default:
      break;
  }

  LayoutBlock* scroll_container =
      LayoutBlock::CreateAnonymousWithParentAndDisplay(this, display);
  LayoutBox::AddChild(scroll_container);
}

void LayoutScrollContainerWithMarkers::UpdateAnonymousChildStyle(
    const LayoutObject*,
    ComputedStyleBuilder& child_style_builder) const {
  // TODO(crbug.com/332396355): Determine which properties to inherit.
  child_style_builder.SetOverflowX(StyleRef().OverflowX());
  child_style_builder.SetOverflowY(StyleRef().OverflowY());
  child_style_builder.SetUnicodeBidi(StyleRef().GetUnicodeBidi());
  child_style_builder.SetHeight(StyleRef().Height());
  child_style_builder.SetWidth(StyleRef().Width());

  // scroll-start
  child_style_builder.SetScrollStartX(StyleRef().ScrollStartX());
  child_style_builder.SetScrollStartY(StyleRef().ScrollStartY());
}

LayoutUnit LayoutScrollContainerWithMarkers::ScrollWidth() const {
  if (const auto* content = FindAnonymousContentBox()) {
    return content->ScrollWidth();
  }
  return LayoutBlockFlow::ScrollWidth();
}

LayoutUnit LayoutScrollContainerWithMarkers::ScrollHeight() const {
  if (const auto* content = FindAnonymousContentBox()) {
    return content->ScrollHeight();
  }
  return LayoutBlockFlow::ScrollHeight();
}

LayoutBox* LayoutScrollContainerWithMarkers::FindScrollMarkerGroup() const {
  LayoutObject* first_child = FirstChild();
  if (!first_child) {
    return nullptr;
  }
  if (first_child->IsScrollMarkerGroup()) {
    return To<LayoutBox>(first_child);
  }
  LayoutObject* last_child = first_child->NextSibling();
  CHECK(!last_child || !last_child->NextSibling());
  if (last_child && last_child->IsScrollMarkerGroup()) {
    return To<LayoutBox>(last_child);
  }
  return nullptr;
}

}  // namespace blink
