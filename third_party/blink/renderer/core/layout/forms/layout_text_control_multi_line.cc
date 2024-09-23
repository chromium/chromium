// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/forms/layout_text_control_multi_line.h"

#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/forms/layout_text_control.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"

namespace blink {

LayoutTextControlMultiLine::LayoutTextControlMultiLine(Element* element)
    : LayoutBlockFlow(element) {}

HTMLElement* LayoutTextControlMultiLine::InnerEditorElement() const {
  return To<TextControlElement>(GetNode())->InnerEditorElement();
}

void LayoutTextControlMultiLine::StyleDidChange(
    StyleDifference style_diff,
    const ComputedStyle* old_style) {
  LayoutBlockFlow::StyleDidChange(style_diff, old_style);
  layout_text_control::StyleDidChange(InnerEditorElement(), old_style,
                                      StyleRef());
}

bool LayoutTextControlMultiLine::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset,
    HitTestPhase phase) {
  bool stop_hit_testing = LayoutBlockFlow::NodeAtPoint(
      result, hit_test_location, accumulated_offset, phase);

  const LayoutObject* stop_node = result.GetHitTestRequest().GetStopNode();
  if (stop_node && stop_node->NodeForHitTest() == result.InnerNode()) {
    return stop_hit_testing;
  }

  HTMLElement* inner_editor = InnerEditorElement();
  if (result.InnerNode() == GetNode() || result.InnerNode() == inner_editor) {
    layout_text_control::HitInnerEditorElement(
        *this, *inner_editor, result, hit_test_location, accumulated_offset);
  }
  return stop_hit_testing;
}

}  // namespace blink
