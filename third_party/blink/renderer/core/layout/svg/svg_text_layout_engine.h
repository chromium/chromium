/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_TEXT_LAYOUT_ENGINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_TEXT_LAYOUT_ENGINE_H_

#include <memory>
#include "third_party/blink/renderer/core/layout/api/line_layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/svg_text_fragment.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ComputedStyle;
class InlineFlowBox;
class PathPositionMapper;
class SVGInlineFlowBox;
class SVGInlineTextBox;
class SVGTextMetrics;

// SVGTextLayoutEngine performs the second layout phase for SVG text.
//
// The InlineBox tree was created, containing the text chunk information,
// necessary to apply certain SVG specific text layout properties (text-length
// adjustments and text-anchor).
// The second layout phase uses the SVGTextLayoutAttributes stored in the
// individual LayoutSVGInlineText layoutObjects to compute the final positions
// for each character which are stored in the SVGInlineTextBox objects.

class SVGTextLayoutEngine {
  STACK_ALLOCATED();

 public:
  SVGTextLayoutEngine(const Vector<LayoutSVGInlineText*>&);
  SVGTextLayoutEngine(const SVGTextLayoutEngine&) = delete;
  SVGTextLayoutEngine& operator=(const SVGTextLayoutEngine&) = delete;
  ~SVGTextLayoutEngine();

  void LayoutCharactersInTextBoxes(InlineFlowBox* start);
  void FinishLayout();

 private:
  bool SetCurrentTextPosition(const SVGCharacterData&);
  void AdvanceCurrentTextPosition(float glyph_advance);
  bool ApplyRelativePositionAdjustmentsIfNeeded(const SVGCharacterData&);

  void ComputeCurrentFragmentMetrics(SVGInlineTextBox*);
  void RecordTextFragment(SVGInlineTextBox*);

  void BeginTextPathLayout(SVGInlineFlowBox*);
  void EndTextPathLayout();

  void LayoutInlineTextBox(SVGInlineTextBox*);
  void LayoutTextOnLineOrPath(SVGInlineTextBox*,
                              LineLayoutSVGInlineText,
                              const ComputedStyle&);

  const LayoutSVGInlineText* NextLogicalTextNode();
  const LayoutSVGInlineText* CurrentLogicalCharacterMetrics(SVGTextMetrics&);
  void AdvanceToNextLogicalCharacter(const SVGTextMetrics&);

  // Logical iteration state.
  const Vector<LayoutSVGInlineText*>& descendant_text_nodes_;
  unsigned current_logical_text_node_index_;
  unsigned logical_character_offset_;
  unsigned logical_metrics_list_offset_;

  Vector<SVGInlineTextBox*> line_layout_boxes_;

  SVGTextFragment current_text_fragment_;
  SVGInlineTextMetricsIterator visual_metrics_iterator_;
  FloatPoint text_position_;
  bool is_vertical_text_;
  bool in_path_layout_;
  bool text_length_spacing_in_effect_;

  // Text on path layout
  std::unique_ptr<PathPositionMapper> text_path_;
  float text_path_start_offset_;
  float text_path_current_offset_;
  float text_path_displacement_;
  float text_path_spacing_;
  float text_path_scaling_;
};

}  // namespace blink

#endif
