/*
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LINE_SVG_INLINE_TEXT_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LINE_SVG_INLINE_TEXT_BOX_H_

#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/svg/svg_text_layout_engine.h"

namespace blink {

class TextMarkerBase;

class SVGInlineTextBox final : public InlineTextBox {
 public:
  SVGInlineTextBox(LineLayoutItem, int start, uint16_t length);

  bool IsSVGInlineTextBox() const override { return true; }

  LayoutUnit VirtualLogicalHeight() const override { return logical_height_; }
  void SetLogicalHeight(LayoutUnit height) { logical_height_ = height; }

  int OffsetForPosition(LayoutUnit x,
                        IncludePartialGlyphsOption,
                        BreakGlyphsOption) const override;
  LayoutUnit PositionForOffset(int offset) const override;

  void Paint(const PaintInfo&,
             const LayoutPoint&,
             LayoutUnit line_top,
             LayoutUnit line_bottom) const override;
  LayoutRect LocalSelectionRect(
      int start_position,
      int end_position,
      bool consider_current_selection = true) const override;

  bool MapStartEndPositionsIntoFragmentCoordinates(const SVGTextFragment&,
                                                   int& start_position,
                                                   int& end_position) const;

  // Calculate the bounding rect of all text fragments.
  FloatRect CalculateBoundaries() const;

  void ClearTextFragments() { text_fragments_.clear(); }
  Vector<SVGTextFragment>& TextFragments() { return text_fragments_; }
  const Vector<SVGTextFragment>& TextFragments() const {
    return text_fragments_;
  }

  void DirtyLineBoxes() override;

  bool StartsNewTextChunk() const { return starts_new_text_chunk_; }
  void SetStartsNewTextChunk(bool new_text_chunk) {
    starts_new_text_chunk_ = new_text_chunk;
  }

  int OffsetForPositionInFragment(const SVGTextFragment&, float position) const;
  FloatRect SelectionRectForTextFragment(const SVGTextFragment&,
                                         int fragment_start_position,
                                         int fragment_end_position,
                                         const ComputedStyle&) const;
  TextRun ConstructTextRun(const ComputedStyle&, const SVGTextFragment&) const;

 private:
  void PaintDocumentMarker(GraphicsContext&,
                           const LayoutPoint&,
                           const DocumentMarker&,
                           const ComputedStyle&,
                           const Font&,
                           bool) const final;
  void PaintTextMarkerForeground(const PaintInfo&,
                                 const LayoutPoint&,
                                 const TextMarkerBase&,
                                 const ComputedStyle&,
                                 const Font&) const final;
  void PaintTextMarkerBackground(const PaintInfo&,
                                 const LayoutPoint&,
                                 const TextMarkerBase&,
                                 const ComputedStyle&,
                                 const Font&) const final;

  bool HitTestFragments(const HitTestLocation& hit_test_location) const;
  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   LayoutUnit line_top,
                   LayoutUnit line_bottom) override;

  LayoutUnit logical_height_;
  bool starts_new_text_chunk_ : 1;
  Vector<SVGTextFragment> text_fragments_;
};

DEFINE_INLINE_BOX_TYPE_CASTS(SVGInlineTextBox);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LINE_SVG_INLINE_TEXT_BOX_H_
