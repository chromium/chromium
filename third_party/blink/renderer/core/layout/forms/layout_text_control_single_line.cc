// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/forms/layout_text_control_single_line.h"

#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/layout/forms/layout_text_control.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"

namespace blink {

LayoutTextControlSingleLine::LayoutTextControlSingleLine(Element* element)
    : LayoutBlockFlow(element) {}

HTMLElement* LayoutTextControlSingleLine::InnerEditorElement() const {
  return To<TextControlElement>(GetNode())->InnerEditorElement();
}

Element* LayoutTextControlSingleLine::ContainerElement() const {
  NOT_DESTROYED();
  return To<Element>(GetNode())->UserAgentShadowRoot()->getElementById(
      shadow_element_names::kIdTextFieldContainer);
}

void LayoutTextControlSingleLine::StyleDidChange(
    StyleDifference style_diff,
    const ComputedStyle* old_style) {
  LayoutBlockFlow::StyleDidChange(style_diff, old_style);
  layout_text_control::StyleDidChange(InnerEditorElement(), old_style,
                                      StyleRef());
}

bool LayoutTextControlSingleLine::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset,
    HitTestPhase phase) {
  NOT_DESTROYED();
  bool stop_hit_testing = LayoutBlockFlow::NodeAtPoint(
      result, hit_test_location, accumulated_offset, phase);

  const LayoutObject* stop_node = result.GetHitTestRequest().GetStopNode();
  if (!result.InnerNode() ||
      (stop_node && stop_node->NodeForHitTest() == result.InnerNode())) {
    return stop_hit_testing;
  }

  // Say that we hit the inner text element if
  //  - we hit a node inside the inner editor element,
  //  - we hit the <input> element (e.g. we're over the border or padding), or
  //  - we hit regions not in any decoration buttons.
  Element* container = ContainerElement();
  HTMLElement* inner_editor = InnerEditorElement();
  if (result.InnerNode()->IsDescendantOf(inner_editor) ||
      result.InnerNode() == GetNode() ||
      (container && container == result.InnerNode())) {
    layout_text_control::HitInnerEditorElement(
        *this, *inner_editor, result, hit_test_location, accumulated_offset);
  }
  return stop_hit_testing;
}

bool LayoutTextControlSingleLine::RespectsCSSOverflow() const {
  NOT_DESTROYED();
  // Do not show scrollbars even if overflow:scroll is specified.
  return false;
}

}  // namespace blink
