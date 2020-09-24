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
                             const HitTestLocation&,
                             const PhysicalOffset& accumulated_offset);

  static bool HasValidAvgCharWidth(const SimpleFontData*,
                                   const AtomicString& family);
  float GetAvgCharWidth(const AtomicString& family) const;
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

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectTextControl || LayoutBlockFlow::IsOfType(type);
  }

 private:
  MinMaxSizes ComputeIntrinsicLogicalWidths() const final;
  void RemoveLeftoverAnonymousBlock(LayoutBlock*) final {}

  void AddOutlineRects(Vector<PhysicalRect>&,
                       const PhysicalOffset& additional_offset,
                       NGOutlineType) const final;

  bool CanBeProgramaticallyScrolled() const final { return true; }
};

template <>
struct DowncastTraits<LayoutTextControl> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsTextControl();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TEXT_CONTROL_H_
