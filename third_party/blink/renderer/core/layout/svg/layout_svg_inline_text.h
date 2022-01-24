/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Rob Buis <buis@kde.org>
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
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_INLINE_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_INLINE_TEXT_H_

#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/svg/svg_character_data.h"
#include "third_party/blink/renderer/core/layout/svg/svg_text_metrics.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class LayoutSVGInlineText final : public LayoutText {
 public:
  LayoutSVGInlineText(Node*, scoped_refptr<StringImpl>);

  bool CharacterStartsNewTextChunk(int position) const;
  SVGCharacterDataMap& CharacterDataMap() {
    NOT_DESTROYED();
    return character_data_map_;
  }
  const SVGCharacterDataMap& CharacterDataMap() const {
    NOT_DESTROYED();
    return character_data_map_;
  }

  const Vector<SVGTextMetrics>& MetricsList() const {
    NOT_DESTROYED();
    return metrics_;
  }

  float ScalingFactor() const {
    NOT_DESTROYED();
    return scaling_factor_;
  }
  const Font& ScaledFont() const {
    NOT_DESTROYED();
    return scaled_font_;
  }
  void UpdateScaledFont();
  void UpdateMetricsList(bool& last_character_was_white_space);
  static void ComputeNewScaledFontForStyle(const LayoutObject&,
                                           float& scaling_factor,
                                           Font& scaled_font);

  // Preserves floating point precision for the use in DRT. It knows how to
  // round and does a better job than enclosingIntRect.
  gfx::RectF FloatLinesBoundingBox() const;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutSVGInlineText";
  }
  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const override;

 private:
  void TextDidChange() override;
  void StyleDidChange(StyleDifference, const ComputedStyle*) override;
  bool IsFontFallbackValid() const override;
  void InvalidateSubtreeLayoutForFontUpdates() override;

  void AddMetricsFromRun(const TextRun&, bool& last_character_was_white_space);

  gfx::RectF ObjectBoundingBox() const override;

  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectSVG || type == kLayoutObjectSVGInlineText ||
           LayoutText::IsOfType(type);
  }

  LayoutRect LocalCaretRect(
      const InlineBox*,
      int caret_offset,
      LayoutUnit* extra_width_to_end_of_line = nullptr) const override;
  PhysicalRect PhysicalLinesBoundingBox() const override;
  InlineTextBox* CreateTextBox(int start, uint16_t length) override;

  PhysicalRect VisualRectInDocument(VisualRectFlags) const final;
  gfx::RectF VisualRectInLocalSVGCoordinates() const final;

  float scaling_factor_;
  Font scaled_font_;
  SVGCharacterDataMap character_data_map_;
  Vector<SVGTextMetrics> metrics_;
};

template <>
struct DowncastTraits<LayoutSVGInlineText> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsSVGInlineText();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_INLINE_TEXT_H_
