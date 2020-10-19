// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_text_control_single_line.h"

#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/layout_text_control.h"

namespace blink {

LayoutNGTextControlSingleLine::LayoutNGTextControlSingleLine(Element* element)
    : LayoutNGBlockFlow(element) {}

HTMLElement* LayoutNGTextControlSingleLine::InnerEditorElement() const {
  return To<TextControlElement>(GetNode())->InnerEditorElement();
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

}  // namespace blink
