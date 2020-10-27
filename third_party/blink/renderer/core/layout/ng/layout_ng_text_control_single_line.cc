// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_text_control_single_line.h"

#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/layout/layout_text_control.h"

namespace blink {

LayoutNGTextControlSingleLine::LayoutNGTextControlSingleLine(Element* element)
    : LayoutNGBlockFlow(element) {}

HTMLElement* LayoutNGTextControlSingleLine::InnerEditorElement() const {
  return To<TextControlElement>(GetNode())->InnerEditorElement();
}

Element* LayoutNGTextControlSingleLine::ContainerElement() const {
  NOT_DESTROYED();
  return To<Element>(GetNode())->UserAgentShadowRoot()->getElementById(
      shadow_element_names::kIdTextFieldContainer);
}

Element* LayoutNGTextControlSingleLine::EditingViewPortElement() const {
  NOT_DESTROYED();
  return To<Element>(GetNode())->UserAgentShadowRoot()->getElementById(
      shadow_element_names::kIdEditingViewPort);
}

bool LayoutNGTextControlSingleLine::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGTextControlSingleLine ||
         LayoutNGBlockFlow::IsOfType(type);
}

void LayoutNGTextControlSingleLine::StyleDidChange(
    StyleDifference style_diff,
    const ComputedStyle* old_style) {
  LayoutNGBlockFlow::StyleDidChange(style_diff, old_style);
  LayoutTextControl::StyleDidChange(InnerEditorElement(), old_style,
                                    StyleRef());
}

bool LayoutNGTextControlSingleLine::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset,
    HitTestAction hit_test_action) {
  NOT_DESTROYED();
  if (!LayoutNGBlockFlow::NodeAtPoint(result, hit_test_location,
                                      accumulated_offset, hit_test_action))
    return false;

  const LayoutObject* stop_node = result.GetHitTestRequest().GetStopNode();
  if (stop_node && stop_node->NodeForHitTest() == result.InnerNode())
    return true;

  // Say that we hit the inner text element if
  //  - we hit a node inside the inner editor element,
  //  - we hit the <input> element (e.g. we're over the border or padding), or
  //  - we hit regions not in any decoration buttons.
  Element* container = ContainerElement();
  HTMLElement* inner_editor = InnerEditorElement();
  Element* view_port = EditingViewPortElement();
  if (result.InnerNode()->IsDescendantOf(inner_editor) ||
      result.InnerNode() == GetNode() ||
      (container && container == result.InnerNode())) {
    PhysicalOffset inner_editor_accumulated_offset = accumulated_offset;
    if (container && view_port) {
      if (auto* view_port_box = view_port->GetLayoutBox())
        inner_editor_accumulated_offset += view_port_box->PhysicalLocation();
      if (auto* container_box = container->GetLayoutBox())
        inner_editor_accumulated_offset += container_box->PhysicalLocation();
    }
    LayoutTextControl::HitInnerEditorElement(
        *this, *inner_editor, result, hit_test_location, accumulated_offset);
  }
  return true;
}

bool LayoutNGTextControlSingleLine::AllowsNonVisibleOverflow() const {
  NOT_DESTROYED();
  // Do not show scrollbars even if overflow:scroll is specified.
  return false;
}

}  // namespace blink
