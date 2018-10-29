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

using namespace HTMLNames;

LayoutTextControlSingleLine::LayoutTextControlSingleLine(
    HTMLInputElement* element)
    : LayoutTextControl(element), should_draw_caps_lock_indicator_(false) {}

LayoutTextControlSingleLine::~LayoutTextControlSingleLine() = default;

inline Element* LayoutTextControlSingleLine::ContainerElement() const {
  return InputElement()->UserAgentShadowRoot()->getElementById(
      ShadowElementNames::TextFieldContainer());
}

inline Element* LayoutTextControlSingleLine::EditingViewPortElement() const {
  return InputElement()->UserAgentShadowRoot()->getElementById(
      ShadowElementNames::EditingViewPort());
}

inline HTMLElement* LayoutTextControlSingleLine::InnerSpinButtonElement()
    const {
  return ToHTMLElement(InputElement()->UserAgentShadowRoot()->getElementById(
      ShadowElementNames::SpinButton()));
}

void LayoutTextControlSingleLine::Paint(const PaintInfo& paint_info) const {
  TextControlSingleLinePainter(*this).Paint(paint_info);
}

void LayoutTextControlSingleLine::UpdateLayout() {
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
    if (needed_layout)
      ComputeOverflow(ClientLogicalBottom());
  }
}

bool LayoutTextControlSingleLine::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& location_in_container,
    const LayoutPoint& accumulated_offset,
    HitTestAction hit_test_action) {
  if (!LayoutTextControl::NodeAtPoint(result, location_in_container,
                                      accumulated_offset, hit_test_action))
    return false;

  // Say that we hit the inner text element if
  //  - we hit a node inside the inner text element,
  //  - we hit the <input> element (e.g. we're over the border or padding), or
  //  - we hit regions not in any decoration buttons.
  Element* container = ContainerElement();
  if (result.InnerNode()->IsDescendantOf(InnerEditorElement()) ||
      result.InnerNode() == GetNode() ||
      (container && container == result.InnerNode())) {
    LayoutPoint point_in_parent = location_in_container.Point();
    if (container && EditingViewPortElement()) {
      if (EditingViewPortElement()->GetLayoutBox())
        point_in_parent -=
            ToLayoutSize(EditingViewPortElement()->GetLayoutBox()->Location());
      if (container->GetLayoutBox())
        point_in_parent -= ToLayoutSize(container->GetLayoutBox()->Location());
    }
    HitInnerEditorElement(result, point_in_parent, accumulated_offset);
  }
  return true;
}

void LayoutTextControlSingleLine::CapsLockStateMayHaveChanged() {
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
        InputElement()->type() == InputTypeNames::password &&
        frame->Selection().FrameIsFocusedAndActive() &&
        GetDocument().FocusedElement() == GetNode() &&
        KeyboardEventManager::CurrentCapsLockState();

  if (should_draw_caps_lock_indicator != should_draw_caps_lock_indicator_) {
    should_draw_caps_lock_indicator_ = should_draw_caps_lock_indicator;
    SetShouldDoFullPaintInvalidation();
  }
}

bool LayoutTextControlSingleLine::HasControlClip() const {
  return true;
}

LayoutRect LayoutTextControlSingleLine::ControlClipRect(
    const LayoutPoint& additional_offset) const {
  LayoutRect clip_rect = PhysicalPaddingBoxRect();
  clip_rect.MoveBy(additional_offset);
  return clip_rect;
}

float LayoutTextControlSingleLine::GetAvgCharWidth(
    const AtomicString& family) const {
  // Match the default system font to the width of MS Shell Dlg, the default
  // font for textareas in Firefox, Safari Win and IE for some encodings (in
  // IE, the default font is encoding specific). 901 is the avgCharWidth value
  // in the OS/2 table for MS Shell Dlg.
  if (LayoutTheme::GetTheme().NeedsHackForTextControlWithFontFamily(family))
    return ScaleEmToUnits(901);

  return LayoutTextControl::GetAvgCharWidth(family);
}

LayoutUnit LayoutTextControlSingleLine::PreferredContentLogicalWidth(
    float char_width) const {
  int factor;
  bool includes_decoration =
      InputElement()->SizeShouldIncludeDecoration(factor);
  if (factor <= 0)
    factor = 20;

  LayoutUnit result = LayoutUnit::FromFloatCeil(char_width * factor);

  float max_char_width = 0.f;
  const Font& font = StyleRef().GetFont();
  AtomicString family = font.GetFontDescription().Family().Family();
  // Match the default system font to the width of MS Shell Dlg, the default
  // font for textareas in Firefox, Safari Win and IE for some encodings (in
  // IE, the default font is encoding specific). 4027 is the (xMax - xMin)
  // value in the "head" font table for MS Shell Dlg.
  if (LayoutTheme::GetTheme().NeedsHackForTextControlWithFontFamily(family))
    max_char_width = ScaleEmToUnits(4027);
  else if (HasValidAvgCharWidth(font.PrimaryFont(), family))
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
  return line_height + non_content_height;
}

void LayoutTextControlSingleLine::Autoscroll(const IntPoint& position) {
  LayoutBox* layout_object = InnerEditorElement()->GetLayoutBox();
  if (!layout_object)
    return;

  layout_object->Autoscroll(position);
}

LayoutUnit LayoutTextControlSingleLine::ScrollWidth() const {
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

LayoutUnit LayoutTextControlSingleLine::ScrollLeft() const {
  if (InnerEditorElement())
    return LayoutUnit(InnerEditorElement()->scrollLeft());
  return LayoutBlockFlow::ScrollLeft();
}

LayoutUnit LayoutTextControlSingleLine::ScrollTop() const {
  if (InnerEditorElement())
    return LayoutUnit(InnerEditorElement()->scrollTop());
  return LayoutBlockFlow::ScrollTop();
}

void LayoutTextControlSingleLine::SetScrollLeft(LayoutUnit new_left) {
  if (InnerEditorElement())
    InnerEditorElement()->setScrollLeft(new_left);
}

void LayoutTextControlSingleLine::SetScrollTop(LayoutUnit new_top) {
  if (InnerEditorElement())
    InnerEditorElement()->setScrollTop(new_top);
}

HTMLInputElement* LayoutTextControlSingleLine::InputElement() const {
  return ToHTMLInputElement(GetNode());
}

void LayoutTextControlSingleLine::ComputeVisualOverflow(
    const LayoutRect& previous_visual_overflow_rect,
    bool recompute_floats) {
  AddVisualOverflowFromChildren();

  AddVisualEffectOverflow();
  AddVisualOverflowFromTheme();

  if (recompute_floats || CreatesNewFormattingContext() ||
      HasSelfPaintingLayer())
    AddVisualOverflowFromFloats();

  if (VisualOverflowRect() != previous_visual_overflow_rect) {
    if (Layer())
      Layer()->SetNeedsCompositingInputsUpdate();
    GetFrameView()->SetIntersectionObservationState(LocalFrameView::kDesired);
  }
}

}  // namespace blink
