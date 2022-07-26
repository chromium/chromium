/**
 * Copyright (C) 2006, 2007, 2010 Apple Inc. All rights reserved.
 *           (C) 2008 Torch Mobile Inc. All rights reserved.
 *               (http://www.torchmobile.com/)
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/layout/layout_text_control_single_line.h"

#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/paint/text_control_single_line_painter.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"

namespace blink {

LayoutTextControlSingleLine::LayoutTextControlSingleLine(Element* element)
    : LayoutTextControl(To<TextControlElement>(element)) {
  DCHECK(IsA<HTMLInputElement>(element));
}

LayoutTextControlSingleLine::~LayoutTextControlSingleLine() = default;

inline Element* LayoutTextControlSingleLine::ContainerElement() const {
  NOT_DESTROYED();
  return InputElement()->UserAgentShadowRoot()->getElementById(
      shadow_element_names::kIdTextFieldContainer);
}

inline Element* LayoutTextControlSingleLine::EditingViewPortElement() const {
  NOT_DESTROYED();
  return InputElement()->UserAgentShadowRoot()->getElementById(
      shadow_element_names::kIdEditingViewPort);
}

inline HTMLElement* LayoutTextControlSingleLine::InnerSpinButtonElement()
    const {
  NOT_DESTROYED();
  return To<HTMLElement>(InputElement()->UserAgentShadowRoot()->getElementById(
      shadow_element_names::kIdSpinButton));
}

void LayoutTextControlSingleLine::Paint(const PaintInfo& paint_info) const {
  NOT_DESTROYED();
  TextControlSingleLinePainter(*this).Paint(paint_info);
}

void LayoutTextControlSingleLine::UpdateLayout() {
  NOT_DESTROYED();

  LayoutBlockFlow::UpdateBlockLayout(true);

  LayoutBox* inner_editor_layout_object = InnerEditorElement()->GetLayoutBox();
  Element* container = ContainerElement();
  LayoutBox* container_layout_object =
      container ? container->GetLayoutBox() : nullptr;
  // Center the child block in the block progression direction (vertical
  // centering for horizontal text fields).
  if (!container && inner_editor_layout_object &&
      inner_editor_layout_object->Size().Height() != ContentLogicalHeight()) {
    LayoutUnit logical_height_diff =
        inner_editor_layout_object->LogicalHeight() - ContentLogicalHeight();
    inner_editor_layout_object->SetLogicalTop(
        inner_editor_layout_object->LogicalTop() -
        (logical_height_diff / 2 + LayoutMod(logical_height_diff, 2)));
  } else if (container && container_layout_object &&
             container_layout_object->Size().Height() !=
                 ContentLogicalHeight()) {
    LayoutUnit logical_height_diff =
        container_layout_object->LogicalHeight() - ContentLogicalHeight();
    container_layout_object->SetLogicalTop(
        container_layout_object->LogicalTop() -
        (logical_height_diff / 2 + LayoutMod(logical_height_diff, 2)));
  }

  HTMLElement* placeholder_element = InputElement()->PlaceholderElement();
  if (LayoutBox* placeholder_box =
          placeholder_element ? placeholder_element->GetLayoutBox() : nullptr) {
    LayoutUnit inner_editor_logical_width;

    if (inner_editor_layout_object)
      inner_editor_logical_width = inner_editor_layout_object->LogicalWidth();
    placeholder_box->SetOverrideLogicalWidth(inner_editor_logical_width);
    bool needed_layout = placeholder_box->NeedsLayout();
    placeholder_box->LayoutIfNeeded();
    LayoutPoint text_offset;
    if (inner_editor_layout_object)
      text_offset = inner_editor_layout_object->Location();
    if (EditingViewPortElement() && EditingViewPortElement()->GetLayoutBox())
      text_offset +=
          ToLayoutSize(EditingViewPortElement()->GetLayoutBox()->Location());
    if (container_layout_object)
      text_offset += ToLayoutSize(container_layout_object->Location());
    if (inner_editor_layout_object) {
      // We use inlineBlockBaseline() for innerEditor because it has no
      // inline boxes when we show the placeholder.
      LayoutUnit inner_editor_baseline =
          inner_editor_layout_object->InlineBlockBaseline(kHorizontalLine);
      // We use firstLineBoxBaseline() for placeholder.
      // TODO(tkent): It's inconsistent with innerEditorBaseline. However
      // placeholderBox->inlineBlockBase() is unexpectedly larger.
      LayoutUnit placeholder_baseline = placeholder_box->FirstLineBoxBaseline();
      text_offset += LayoutSize(LayoutUnit(),
                                inner_editor_baseline - placeholder_baseline);
    }
    placeholder_box->SetLocation(text_offset);

    // The placeholder gets layout last, after the parent text control and its
    // other children, so in order to get the correct overflow from the
    // placeholder we need to recompute it now.
    if (needed_layout) {
      SetNeedsOverflowRecalc();
      ComputeLayoutOverflow(ClientLogicalBottom());
    }
  }
}

bool LayoutTextControlSingleLine::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset,
    HitTestPhase phase) {
  NOT_DESTROYED();
  if (!LayoutTextControl::NodeAtPoint(result, hit_test_location,
                                      accumulated_offset, phase))
    return false;

  const LayoutObject* stop_node = result.GetHitTestRequest().GetStopNode();
  if (stop_node && stop_node->NodeForHitTest() == result.InnerNode())
    return true;

  // Say that we hit the inner text element if
  //  - we hit a node inside the inner text element,
  //  - we hit the <input> element (e.g. we're over the border or padding), or
  //  - we hit regions not in any decoration buttons.
  Element* container = ContainerElement();
  if (result.InnerNode()->IsDescendantOf(InnerEditorElement()) ||
      result.InnerNode() == GetNode() ||
      (container && container == result.InnerNode())) {
    HitInnerEditorElement(*this, *InnerEditorElement(), result,
                          hit_test_location, accumulated_offset);
  }
  return true;
}

HTMLInputElement* LayoutTextControlSingleLine::InputElement() const {
  NOT_DESTROYED();
  return To<HTMLInputElement>(GetNode());
}

void LayoutTextControlSingleLine::ComputeVisualOverflow(bool recompute_floats) {
  NOT_DESTROYED();
  LayoutRect previous_visual_overflow_rect = VisualOverflowRect();
  ClearVisualOverflow();
  AddVisualOverflowFromChildren();
  AddVisualEffectOverflow();

  if (recompute_floats || CreatesNewFormattingContext() ||
      HasSelfPaintingLayer())
    AddVisualOverflowFromFloats();

  if (VisualOverflowRect() != previous_visual_overflow_rect) {
    InvalidateIntersectionObserverCachedRects();
    SetShouldCheckForPaintInvalidation();
    GetFrameView()->SetIntersectionObservationState(LocalFrameView::kDesired);
  }
}

}  // namespace blink
