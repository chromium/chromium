/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2008 Torch Mobile Inc. All rights reserved.
 *               (http://www.torchmobile.com/)
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TEXT_CONTROL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TEXT_CONTROL_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"

namespace blink {

class TextControlElement;
class TextControlInnerEditorElement;

class CORE_EXPORT LayoutTextControl : public LayoutBlockFlow {
 public:
  ~LayoutTextControl() override;

  TextControlElement* GetTextControlElement() const;
  const char* GetName() const override { return "LayoutTextControl"; }

  bool CreatesNewFormattingContext() const final {
    // INPUT and other replaced elements rendered by Blink itself should be
    // completely contained.
    return true;
  }

 protected:
  LayoutTextControl(TextControlElement*);

  // This convenience function should not be made public because
  // innerEditorElement may outlive the layout tree.
  TextControlInnerEditorElement* InnerEditorElement() const;

  int ScrollbarThickness() const;

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  void HitInnerEditorElement(HitTestResult&,
                             const LayoutPoint& point_in_container,
                             const LayoutPoint& accumulated_offset);

  int TextBlockLogicalWidth() const;
  int TextBlockLogicalHeight() const;

  float ScaleEmToUnits(int x) const;

  static bool HasValidAvgCharWidth(const SimpleFontData*,
                                   const AtomicString& family);
  virtual float GetAvgCharWidth(const AtomicString& family) const;
  virtual LayoutUnit PreferredContentLogicalWidth(float char_width) const = 0;
  virtual LayoutUnit ComputeControlLogicalHeight(
      LayoutUnit line_height,
      LayoutUnit non_content_height) const = 0;

  void ComputeLogicalHeight(LayoutUnit logical_height,
                            LayoutUnit logical_top,
                            LogicalExtentComputedValues&) const override;
  LayoutObject* LayoutSpecialExcludedChild(bool relayout_children,
                                           SubtreeLayoutScope&) override;

  LayoutUnit FirstLineBoxBaseline() const override;
  // We need to override this function because we don't want overflow:hidden on
  // an <input> to affect the baseline calculation. This is necessary because we
  // are an inline-block element as an implementation detail which would
  // normally be affected by this.
  bool ShouldIgnoreOverflowPropertyForInlineBlockBaseline() const override {
    return true;
  }

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectTextControl || LayoutBlockFlow::IsOfType(type);
  }

 private:
  void ComputeIntrinsicLogicalWidths(LayoutUnit& min_logical_width,
                                     LayoutUnit& max_logical_width) const final;
  void ComputePreferredLogicalWidths() final;
  void RemoveLeftoverAnonymousBlock(LayoutBlock*) final {}

  void AddOutlineRects(Vector<LayoutRect>&,
                       const LayoutPoint& additional_offset,
                       NGOutlineType) const final;

  bool CanBeProgramaticallyScrolled() const final { return true; }
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutTextControl, IsTextControl());

// LayoutObject for our inner container, for <search> and others.
// We can't use LayoutFlexibleBox directly, because flexboxes have a different
// baseline definition, and then inputs of different types wouldn't line up
// anymore.
class LayoutTextControlInnerContainer final : public LayoutFlexibleBox {
 public:
  explicit LayoutTextControlInnerContainer(Element* element)
      : LayoutFlexibleBox(element) {}
  ~LayoutTextControlInnerContainer() override = default;

  LayoutUnit BaselinePosition(FontBaseline baseline,
                              bool first_line,
                              LineDirectionMode direction,
                              LinePositionMode position) const override {
    return LayoutBlock::BaselinePosition(baseline, first_line, direction,
                                         position);
  }
  LayoutUnit FirstLineBoxBaseline() const override {
    return LayoutBlock::FirstLineBoxBaseline();
  }
  LayoutUnit InlineBlockBaseline(LineDirectionMode direction) const override {
    return LayoutBlock::InlineBlockBaseline(direction);
  }
  bool ShouldIgnoreOverflowPropertyForInlineBlockBaseline() const override {
    return true;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TEXT_CONTROL_H_
