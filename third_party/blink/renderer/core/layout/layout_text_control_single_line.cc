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
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/input/keyboard_event_manager.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/text_control_single_line_painter.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"

namespace blink {

LayoutTextControlSingleLine::LayoutTextControlSingleLine(
    HTMLInputElement* element)
    : LayoutTextControl(element), should_draw_caps_lock_indicator_(false) {}

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
  LayoutAnalyzer::Scope analyzer(*this);

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
    HitTestAction hit_test_action) {
  NOT_DESTROYED();
  if (!LayoutTextControl::NodeAtPoint(result, hit_test_location,
                                      accumulated_offset, hit_test_action))
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
    PhysicalOffset inner_editor_accumulated_offset = accumulated_offset;
    if (container && EditingViewPortElement()) {
      if (EditingViewPortElement()->GetLayoutBox()) {
        inner_editor_accumulated_offset +=
            EditingViewPortElement()->GetLayoutBox()->PhysicalLocation();
      }
      if (container->GetLayoutBox()) {
        inner_editor_accumulated_offset +=
            container->GetLayoutBox()->PhysicalLocation();
      }
    }
    HitInnerEditorElement(result, hit_test_location, accumulated_offset);
  }
  return true;
}

void LayoutTextControlSingleLine::CapsLockStateMayHaveChanged() {
  NOT_DESTROYED();
  if (!GetNode())
    return;

  // Only draw the caps lock indicator if these things are true:
  // 1) The field is a password field
  // 2) The frame is active
  // 3) The element is focused
  // 4) The caps lock is on
  bool should_draw_caps_lock_indicator = false;

  if (LocalFrame* frame = GetDocument().GetFrame())
    should_draw_caps_lock_indicator =
        InputElement()->type() == input_type_names::kPassword &&
        frame->Selection().FrameIsFocusedAndActive() &&
        GetDocument().FocusedElement() == GetNode() &&
        KeyboardEventManager::CurrentCapsLockState();

  if (should_draw_caps_lock_indicator != should_draw_caps_lock_indicator_) {
    should_draw_caps_lock_indicator_ = should_draw_caps_lock_indicator;
    SetShouldDoFullPaintInvalidation();
  }
}

LayoutUnit LayoutTextControlSingleLine::PreferredContentLogicalWidth(
    float char_width) const {
  NOT_DESTROYED();
  int factor;
  bool includes_decoration =
      InputElement()->SizeShouldIncludeDecoration(factor);
  if (factor <= 0)
    factor = 20;

  LayoutUnit result = LayoutUnit::FromFloatCeil(char_width * factor);

  float max_char_width = 0.f;
  const Font& font = StyleRef().GetFont();
  AtomicString family = font.GetFontDescription().Family().Family();
  if (HasValidAvgCharWidth(font.PrimaryFont(), family))
    max_char_width = roundf(font.PrimaryFont()->MaxCharWidth());

  // For text inputs, IE adds some extra width.
  if (max_char_width > 0.f)
    result += max_char_width - char_width;

  if (includes_decoration) {
    HTMLElement* spin_button = InnerSpinButtonElement();
    if (LayoutBox* spin_layout_object =
            spin_button ? spin_button->GetLayoutBox() : nullptr) {
      result += spin_layout_object->BorderAndPaddingLogicalWidth();
      // Since the width of spin_layout_object is not calculated yet,
      // spin_layout_object->LogicalWidth() returns 0. Use the computed logical
      // width instead.
      result += spin_layout_object->StyleRef().LogicalWidth().Value();
    }
  }

  return result;
}

LayoutUnit LayoutTextControlSingleLine::ComputeControlLogicalHeight(
    LayoutUnit line_height,
    LayoutUnit non_content_height) const {
  NOT_DESTROYED();
  return line_height + non_content_height;
}

LayoutUnit LayoutTextControlSingleLine::ScrollWidth() const {
  NOT_DESTROYED();
  // If in preview state, fake the scroll width to prevent that any information
  // about the suggested content can be derived from the size.
  if (!GetTextControlElement()->SuggestedValue().IsEmpty())
    return ClientWidth();

  if (LayoutBox* inner = InnerEditorElement()
                             ? InnerEditorElement()->GetLayoutBox()
                             : nullptr) {
    // Adjust scrollWidth to inculde input element horizontal paddings and
    // decoration width
    LayoutUnit adjustment = ClientWidth() - inner->ClientWidth();
    return inner->ScrollWidth() + adjustment;
  }
  return LayoutBlockFlow::ScrollWidth();
}

LayoutUnit LayoutTextControlSingleLine::ScrollHeight() const {
  NOT_DESTROYED();
  // If in preview state, fake the scroll height to prevent that any information
  // about the suggested content can be derived from the size.
  if (!GetTextControlElement()->SuggestedValue().IsEmpty())
    return ClientHeight();

  if (LayoutBox* inner = InnerEditorElement()
                             ? InnerEditorElement()->GetLayoutBox()
                             : nullptr) {
    // Adjust scrollHeight to include input element vertical paddings and
    // decoration height
    LayoutUnit adjustment = ClientHeight() - inner->ClientHeight();
    return inner->ScrollHeight() + adjustment;
  }
  return LayoutBlockFlow::ScrollHeight();
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
