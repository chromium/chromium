/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 *               (http://www.torchmobile.com/)
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TEXT_CONTROL_SINGLE_LINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TEXT_CONTROL_SINGLE_LINE_H_

#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/layout/layout_text_control.h"

namespace blink {

class HTMLInputElement;

// LayoutObject for text-field <input>s.
//
// This class inherits from LayoutTextControl and LayoutBlockFlow. If we'd like
// to change the base class, we need to make sure that
// ShouldIgnoreOverflowPropertyForInlineBlockBaseline flag works with the new
// base class.
class LayoutTextControlSingleLine : public LayoutTextControl {
 public:
  LayoutTextControlSingleLine(HTMLInputElement*);
  ~LayoutTextControlSingleLine() override;

  void CapsLockStateMayHaveChanged();
  bool ShouldDrawCapsLockIndicator() const {
    return should_draw_caps_lock_indicator_;
  }

 protected:
  Element* ContainerElement() const;
  Element* EditingViewPortElement() const;
  HTMLInputElement* InputElement() const;

 private:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectTextField || LayoutTextControl::IsOfType(type);
  }

  void Paint(const PaintInfo&) const override;
  void UpdateLayout() override;

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   HitTestAction) final;

  // Subclassed to forward to our inner div.
  LayoutUnit ScrollWidth() const final;
  LayoutUnit ScrollHeight() const final;

  int TextBlockWidth() const;
  LayoutUnit PreferredContentLogicalWidth(float char_width) const final;
  LayoutUnit ComputeControlLogicalHeight(
      LayoutUnit line_height,
      LayoutUnit non_content_height) const override;

  void ComputeVisualOverflow(bool recompute_floats) override;

  // If the INPUT content height is smaller than the font height, the
  // inner-editor element overflows the INPUT box intentionally, however it
  // shouldn't affect outside of the INPUT box.  So we ignore child overflow.
  void AddLayoutOverflowFromChildren() final {}

  bool AllowsNonVisibleOverflow() const override { return false; }

  HTMLElement* InnerSpinButtonElement() const;

  bool should_draw_caps_lock_indicator_;
};

template <>
struct DowncastTraits<LayoutTextControlSingleLine> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsTextField();
  }
};

// ----------------------------

class LayoutTextControlInnerEditor : public LayoutBlockFlow {
 public:
  LayoutTextControlInnerEditor(Element* element) : LayoutBlockFlow(element) {}

 private:
  bool IsIntrinsicallyScrollable(
      ScrollbarOrientation orientation) const override {
    return orientation == kHorizontalScrollbar;
  }
  bool ScrollsOverflowX() const override { return IsScrollContainer(); }
  bool ScrollsOverflowY() const override { return false; }
};

}  // namespace blink

#endif
