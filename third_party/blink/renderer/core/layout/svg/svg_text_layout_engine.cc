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

#include "third_party/blink/renderer/core/layout/svg/svg_text_layout_engine.h"

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_svg_text_path.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/line/svg_inline_flow_box.h"
#include "third_party/blink/renderer/core/layout/svg/line/svg_inline_text_box.h"
#include "third_party/blink/renderer/core/layout/svg/svg_text_chunk_builder.h"
#include "third_party/blink/renderer/core/layout/svg/svg_text_layout_engine_baseline.h"
#include "third_party/blink/renderer/core/layout/svg/svg_text_layout_engine_spacing.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg/svg_text_content_element.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

SVGTextLayoutEngine::SVGTextLayoutEngine(
    const HeapVector<Member<LayoutSVGInlineText>>& descendant_text_nodes)
    : descendant_text_nodes_(descendant_text_nodes),
      current_logical_text_node_index_(0),
      logical_character_offset_(0),
      logical_metrics_list_offset_(0),
      is_vertical_text_(false),
      in_path_layout_(false),
      text_length_spacing_in_effect_(false),
      last_text_box_was_in_text_path_(false),
      text_path_(nullptr),
      text_path_current_offset_(0),
      text_path_displacement_(0),
      text_path_spacing_(0),
      text_path_scaling_(1) {
  DCHECK(!descendant_text_nodes_.empty());
}

SVGTextLayoutEngine::~SVGTextLayoutEngine() = default;

bool SVGTextLayoutEngine::SetCurrentTextPosition(const SVGCharacterData& data) {
  bool has_x = data.HasX();
  if (has_x)
    text_position_.set_x(data.x);

  bool has_y = data.HasY();
  if (has_y)
    text_position_.set_y(data.y);

  // If there's an absolute x/y position available, it marks the beginning of
  // a new position along the path.
  if (in_path_layout_) {
    // TODO(fs): If a new chunk (== absolute position) is defined while in
    // path layout mode, alignment should be based on that chunk and not
    // the path as a whole. (Re: the addition of m_textPathStartOffset
    // below.)
    if (is_vertical_text_) {
      if (has_y)
        text_path_current_offset_ = data.y + text_path_start_offset_;
    } else {
      if (has_x)
        text_path_current_offset_ = data.x + text_path_start_offset_;
    }
  } else if ((!has_x || !has_y) && last_text_box_was_in_text_path_) {
    UseCounter::Count(descendant_text_nodes_[0]->GetDocument(),
                      WebFeature::kSVGTextHangingFromPath);
    last_text_box_was_in_text_path_ = false;
  }
  return has_x || has_y;
}

void SVGTextLayoutEngine::AdvanceCurrentTextPosition(float glyph_advance) {
  // TODO(fs): m_textPathCurrentOffset should preferably also be updated
  // here, but that requires a bit more untangling yet.
  if (is_vertical_text_)
    text_position_.set_y(text_position_.y() + glyph_advance);
  else
    text_position_.set_x(text_position_.x() + glyph_advance);
}

bool SVGTextLayoutEngine::ApplyRelativePositionAdjustmentsIfNeeded(
    const SVGCharacterData& data) {
  gfx::Vector2dF delta;
  bool has_dx = data.HasDx();
  if (has_dx)
    delta.set_x(data.dx);

  bool has_dy = data.HasDy();
  if (has_dy)
    delta.set_y(data.dy);

  // Apply dx/dy value adjustments to current text position, if needed.
  text_position_ += delta;

  if (in_path_layout_) {
    if (is_vertical_text_)
      delta.Transpose();

    text_path_current_offset_ += delta.x();
    text_path_displacement_ += delta.y();
  }
  return has_dx || has_dy;
}

void SVGTextLayoutEngine::ComputeCurrentFragmentMetrics(
    SVGInlineTextBox* text_box) {
  LineLayoutSVGInlineText text_line_layout =
      LineLayoutSVGInlineText(text_box->GetLineLayoutItem());
  TextRun run = text_box->ConstructTextRun(text_line_layout.StyleRef(),
                                           current_text_fragment_);

  float scaling_factor = text_line_layout.ScalingFactor();
  DCHECK(scaling_factor);
  const Font& scaled_font = text_line_layout.ScaledFont();
  gfx::RectF glyph_overflow_bounds;

  const SimpleFontData* font_data = scaled_font.PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return;

  float width = scaled_font.Width(run, nullptr, &glyph_overflow_bounds);
  current_text_fragment_.glyph_overflow.SetFromBounds(glyph_overflow_bounds,
                                                      scaled_font, width);
  current_text_fragment_.glyph_overflow.top /= scaling_factor;
  current_text_fragment_.glyph_overflow.left /= scaling_factor;
  current_text_fragment_.glyph_overflow.right /= scaling_factor;
  current_text_fragment_.glyph_overflow.bottom /= scaling_factor;

  float height = font_data->GetFontMetrics().FloatHeight();
  current_text_fragment_.height = height / scaling_factor;
  current_text_fragment_.width = width / scaling_factor;
}

void SVGTextLayoutEngine::RecordTextFragment(SVGInlineTextBox* text_box) {
  DCHECK(!current_text_fragment_.length);

  // Figure out length of fragment.
  current_text_fragment_.length = visual_metrics_iterator_.CharacterOffset() -
                                  current_text_fragment_.character_offset;

  // Figure out fragment metrics.
  ComputeCurrentFragmentMetrics(text_box);

  text_box->TextFragments().push_back(current_text_fragment_);
  current_text_fragment_ = SVGTextFragment();
}

void SVGTextLayoutEngine::BeginTextPathLayout(SVGInlineFlowBox* flow_box) {
  // Build text chunks for all <textPath> children, using the line layout
  // algorithm. This is needeed as text-anchor is just an additional startOffset
  // for text paths.
  SVGTextLayoutEngine line_layout(descendant_text_nodes_);
  line_layout.text_length_spacing_in_effect_ = text_length_spacing_in_effect_;
  line_layout.LayoutCharactersInTextBoxes(flow_box);

  in_path_layout_ = true;
  LineLayoutSVGTextPath text_path =
      LineLayoutSVGTextPath(flow_box->GetLineLayoutItem());

  text_path_ = text_path.LayoutPath();
  if (!text_path_)
    return;
  text_path_start_offset_ = text_path_->StartOffset();

  SVGTextPathChunkBuilder text_path_chunk_layout_builder;
  text_path_chunk_layout_builder.ProcessTextChunks(
      line_layout.line_layout_boxes_);

  // Handle 'textLength' adjustments.
  SVGLengthAdjustType length_adjust = kSVGLengthAdjustUnknown;
  float desired_text_length = 0;

  if (SVGTextContentElement* text_content_element =
          SVGTextContentElement::ElementFromLineLayoutItem(text_path)) {
    SVGLengthContext length_context(text_content_element);
    length_adjust = text_content_element->lengthAdjust()->CurrentEnumValue();
    if (text_content_element->TextLengthIsSpecifiedByUser())
      desired_text_length =
          text_content_element->textLength()->CurrentValue()->Value(
              length_context);
    else
      desired_text_length = 0;
  }

  float text_path_content_length = text_path_chunk_layout_builder.TotalLength();
  if (desired_text_length) {
    if (length_adjust == kSVGLengthAdjustSpacing) {
      text_path_spacing_ = 0;
      if (text_path_chunk_layout_builder.TotalCharacters() > 1) {
        text_path_spacing_ = desired_text_length - text_path_content_length;
        text_path_spacing_ /=
            text_path_chunk_layout_builder.TotalCharacters() - 1;
      }
    } else {
      text_path_scaling_ = desired_text_length / text_path_content_length;
    }
    text_path_content_length = desired_text_length;
  }

  // Perform text-anchor adjustment.
  float text_anchor_shift =
      CalculateTextAnchorShift(text_path.StyleRef(), text_path_content_length);
  text_path_start_offset_ += text_anchor_shift;
  text_path_current_offset_ = text_path_start_offset_;
}

void SVGTextLayoutEngine::EndTextPathLayout() {
  in_path_layout_ = false;
  text_path_ = nullptr;
  text_path_start_offset_ = 0;
  text_path_current_offset_ = 0;
  text_path_spacing_ = 0;
  text_path_scaling_ = 1;
}

void SVGTextLayoutEngine::LayoutInlineTextBox(SVGInlineTextBox* text_box) {
  DCHECK(text_box);

  LineLayoutSVGInlineText text_line_layout =
      LineLayoutSVGInlineText(text_box->GetLineLayoutItem());
  DCHECK(text_line_layout.Parent());
  DCHECK(text_line_layout.Parent().GetNode());
  DCHECK(text_line_layout.Parent().GetNode()->IsSVGElement());

  const ComputedStyle& style = text_line_layout.StyleRef();

  text_box->ClearTextFragments();
  is_vertical_text_ = !style.IsHorizontalWritingMode();
  LayoutTextOnLineOrPath(text_box, text_line_layout, style);

  if (in_path_layout_)
    return;

  line_layout_boxes_.push_back(text_box);
}

static bool DefinesTextLengthWithSpacing(const InlineFlowBox* start) {
  SVGTextContentElement* text_content_element =
      SVGTextContentElement::ElementFromLineLayoutItem(
          start->GetLineLayoutItem());
  return text_content_element &&
         text_content_element->lengthAdjust()->CurrentEnumValue() ==
             kSVGLengthAdjustSpacing &&
         text_content_element->TextLengthIsSpecifiedByUser();
}

void SVGTextLayoutEngine::LayoutCharactersInTextBoxes(InlineFlowBox* start) {
  bool text_length_spacing_in_effect =
      text_length_spacing_in_effect_ || DefinesTextLengthWithSpacing(start);
  base::AutoReset<bool> text_length_spacing_scope(
      &text_length_spacing_in_effect_, text_length_spacing_in_effect);
  last_text_box_was_in_text_path_ = false;

  for (InlineBox* child = start->FirstChild(); child;
       child = child->NextOnLine()) {
    if (auto* svg_inline_text_box = DynamicTo<SVGInlineTextBox>(child)) {
      DCHECK(child->GetLineLayoutItem().IsSVGInlineText());
      LayoutInlineTextBox(svg_inline_text_box);
      last_text_box_was_in_text_path_ = false;
    } else {
      // Skip generated content.
      Node* node = child->GetLineLayoutItem().GetNode();
      if (!node)
        continue;

      auto* flow_box = To<SVGInlineFlowBox>(child);
      bool is_text_path = IsA<SVGTextPathElement>(*node);
      if (is_text_path)
        BeginTextPathLayout(flow_box);

      LayoutCharactersInTextBoxes(flow_box);

      if (is_text_path)
        EndTextPathLayout();
      last_text_box_was_in_text_path_ = is_text_path;
    }
  }
}

void SVGTextLayoutEngine::FinishLayout() {
  visual_metrics_iterator_ = SVGInlineTextMetricsIterator();

  // After all text fragments are stored in their correpsonding
  // SVGInlineTextBoxes, we can layout individual text chunks.
  // Chunk layouting is only performed for line layout boxes, not for path
  // layout, where it has already been done.
  SVGTextChunkBuilder chunk_layout_builder;
  chunk_layout_builder.ProcessTextChunks(line_layout_boxes_);

  line_layout_boxes_.clear();
}

const LayoutSVGInlineText* SVGTextLayoutEngine::NextLogicalTextNode() {
  DCHECK_LT(current_logical_text_node_index_, descendant_text_nodes_.size());
  ++current_logical_text_node_index_;
  if (current_logical_text_node_index_ == descendant_text_nodes_.size())
    return nullptr;

  logical_metrics_list_offset_ = 0;
  logical_character_offset_ = 0;
  return descendant_text_nodes_[current_logical_text_node_index_];
}

const LayoutSVGInlineText* SVGTextLayoutEngine::CurrentLogicalCharacterMetrics(
    SVGTextMetrics& logical_metrics) {
  // If we've consumed all text nodes, there can be no more metrics.
  if (current_logical_text_node_index_ == descendant_text_nodes_.size())
    return nullptr;

  const LayoutSVGInlineText* logical_text_node =
      descendant_text_nodes_[current_logical_text_node_index_];
  const Vector<SVGTextMetrics>* metrics_list =
      &logical_text_node->MetricsList();
  unsigned metrics_list_size = metrics_list->size();
  DCHECK_LE(logical_metrics_list_offset_, metrics_list_size);

  // Find the next non-collapsed text metrics cell.
  while (true) {
    // If we run out of metrics, move to the next set of non-empty layout
    // attributes.
    if (logical_metrics_list_offset_ == metrics_list_size) {
      logical_text_node = NextLogicalTextNode();
      if (!logical_text_node)
        return nullptr;
      metrics_list = &logical_text_node->MetricsList();
      metrics_list_size = metrics_list->size();
      // Return to the while so that we check if the new metrics list is
      // non-empty before using it.
      continue;
    }

    DCHECK(metrics_list_size);
    logical_metrics = metrics_list->at(logical_metrics_list_offset_);
    // Stop if we found the next valid logical text metrics object.
    if (!logical_metrics.IsEmpty())
      break;

    AdvanceToNextLogicalCharacter(logical_metrics);
  }

  return logical_text_node;
}

void SVGTextLayoutEngine::AdvanceToNextLogicalCharacter(
    const SVGTextMetrics& logical_metrics) {
  ++logical_metrics_list_offset_;
  logical_character_offset_ += logical_metrics.length();
}

void SVGTextLayoutEngine::LayoutTextOnLineOrPath(
    SVGInlineTextBox* text_box,
    LineLayoutSVGInlineText text_line_layout,
    const ComputedStyle& style) {
  if (in_path_layout_ && !text_path_)
    return;

  // Find the start of the current text box in the metrics list.
  visual_metrics_iterator_.AdvanceToTextStart(text_line_layout,
                                              text_box->Start());

  const Font& font = style.GetFont();

  SVGTextLayoutEngineSpacing spacing_layout(font, style.EffectiveZoom());
  SVGTextLayoutEngineBaseline baseline_layout(text_line_layout.ScaledFont(),
                                              text_line_layout.ScalingFactor());

  bool did_start_text_fragment = false;
  bool apply_spacing_to_next_character = false;
  bool needs_fragment_per_glyph =
      is_vertical_text_ || in_path_layout_ || text_length_spacing_in_effect_;

  float last_angle = 0;
  float baseline_shift_value = baseline_layout.CalculateBaselineShift(style);
  baseline_shift_value -= baseline_layout.CalculateAlignmentBaselineShift(
      is_vertical_text_, text_line_layout);
  gfx::Vector2dF baseline_shift;
  if (is_vertical_text_)
    baseline_shift.set_x(baseline_shift_value);
  else
    baseline_shift.set_y(-baseline_shift_value);

  // Main layout algorithm.
  const unsigned box_end_offset = text_box->Start() + text_box->Len();
  while (!visual_metrics_iterator_.IsAtEnd() &&
         visual_metrics_iterator_.CharacterOffset() < box_end_offset) {
    const SVGTextMetrics& visual_metrics = visual_metrics_iterator_.Metrics();
    if (visual_metrics.IsEmpty()) {
      visual_metrics_iterator_.Next();
      continue;
    }

    SVGTextMetrics logical_metrics(SVGTextMetrics::kSkippedSpaceMetrics);
    const LayoutSVGInlineText* logical_text_node =
        CurrentLogicalCharacterMetrics(logical_metrics);
    if (!logical_text_node)
      break;

    auto it = logical_text_node->CharacterDataMap().find(
        logical_character_offset_ + 1);
    const SVGCharacterData data =
        it != logical_text_node->CharacterDataMap().end() ? it->value
                                                          : SVGCharacterData();

    // TODO(fs): Use the return value to eliminate the additional
    // hash-lookup below when determining if this text box should be tagged
    // as starting a new text chunk.
    SetCurrentTextPosition(data);

    // When we've advanced to the box start offset, determine using the original
    // x/y values, whether this character starts a new text chunk, before doing
    // any further processing.
    if (visual_metrics_iterator_.CharacterOffset() == text_box->Start())
      text_box->SetStartsNewTextChunk(
          logical_text_node->CharacterStartsNewTextChunk(
              logical_character_offset_));

    bool has_relative_position = ApplyRelativePositionAdjustmentsIfNeeded(data);

    // Determine the orientation of the current glyph.
    // Font::width() calculates the resolved FontOrientation for each character,
    // but that value is not exposed today to avoid the API complexity.
    UChar32 current_character = text_line_layout.CodepointAt(
        visual_metrics_iterator_.CharacterOffset());
    FontOrientation font_orientation = font.GetFontDescription().Orientation();
    font_orientation = AdjustOrientationForCharacterInMixedVertical(
        font_orientation, current_character);

    // Calculate glyph advance. The shaping engine takes care of x/y orientation
    // shifts for different fontOrientation values.
    float glyph_advance = visual_metrics.Advance(font_orientation);

    // Calculate CSS 'letter-spacing' and 'word-spacing' for the character, if
    // needed.
    float spacing = spacing_layout.CalculateCSSSpacing(current_character);

    gfx::Vector2dF text_path_shift;
    PointAndTangent position;
    if (in_path_layout_) {
      float scaled_glyph_advance = glyph_advance * text_path_scaling_;
      // Setup translations that move to the glyph midpoint.
      text_path_shift =
          gfx::Vector2dF(-scaled_glyph_advance / 2, text_path_displacement_);
      if (is_vertical_text_)
        text_path_shift.Transpose();
      text_path_shift += baseline_shift;

      // Calculate current offset along path.
      float text_path_offset =
          text_path_current_offset_ + scaled_glyph_advance / 2;

      // Move to next character.
      text_path_current_offset_ += scaled_glyph_advance + text_path_spacing_ +
                                   spacing * text_path_scaling_;

      PathPositionMapper::PositionType position_type =
          text_path_->PointAndNormalAtLength(text_path_offset, position);

      // Skip character, if we're before the path.
      if (position_type == PathPositionMapper::kBeforePath) {
        AdvanceToNextLogicalCharacter(logical_metrics);
        visual_metrics_iterator_.Next();
        continue;
      }

      // Stop processing if the next character lies behind the path.
      if (position_type == PathPositionMapper::kAfterPath)
        break;

      text_position_ = position.point;

      // For vertical text on path, the actual angle has to be rotated 90
      // degrees anti-clockwise, not the orientation angle!
      if (is_vertical_text_)
        position.tangent_in_degrees -= 90;
    } else {
      position.point = text_position_;
      position.point += baseline_shift;
    }

    if (data.HasRotate())
      position.tangent_in_degrees += data.rotate;

    // Determine whether we have to start a new fragment.
    bool should_start_new_fragment =
        needs_fragment_per_glyph || has_relative_position ||
        position.tangent_in_degrees ||
        position.tangent_in_degrees != last_angle ||
        apply_spacing_to_next_character;

    // If we already started a fragment, close it now.
    if (did_start_text_fragment && should_start_new_fragment) {
      apply_spacing_to_next_character = false;
      RecordTextFragment(text_box);
    }

    // Eventually start a new fragment, if not yet done.
    if (!did_start_text_fragment || should_start_new_fragment) {
      DCHECK(!current_text_fragment_.character_offset);
      DCHECK(!current_text_fragment_.length);

      did_start_text_fragment = true;
      current_text_fragment_.character_offset =
          visual_metrics_iterator_.CharacterOffset();
      current_text_fragment_.metrics_list_offset =
          visual_metrics_iterator_.MetricsListOffset();
      current_text_fragment_.x = position.point.x();
      current_text_fragment_.y = position.point.y();

      // Build fragment transformation.
      if (position.tangent_in_degrees)
        current_text_fragment_.transform.Rotate(position.tangent_in_degrees);

      if (text_path_shift.x() || text_path_shift.y()) {
        current_text_fragment_.transform.Translate(text_path_shift.x(),
                                                   text_path_shift.y());
      }

      // For vertical text, always rotate by 90 degrees regardless of
      // fontOrientation.
      // The shaping engine takes care of the necessary orientation.
      if (is_vertical_text_)
        current_text_fragment_.transform.Rotate(90);

      current_text_fragment_.is_vertical = is_vertical_text_;
      current_text_fragment_.is_text_on_path =
          in_path_layout_ && text_path_scaling_ != 1;
      if (current_text_fragment_.is_text_on_path)
        current_text_fragment_.length_adjust_scale = text_path_scaling_;
    }

    // Advance current text position after processing of the current character
    // finished.
    AdvanceCurrentTextPosition(glyph_advance + spacing);

    // Apply CSS 'letter-spacing' and 'word-spacing' to the next character, if
    // needed.
    if (!in_path_layout_ && spacing)
      apply_spacing_to_next_character = true;

    AdvanceToNextLogicalCharacter(logical_metrics);
    visual_metrics_iterator_.Next();
    last_angle = position.tangent_in_degrees;
  }

  if (!did_start_text_fragment)
    return;

  // Close last open fragment, if needed.
  RecordTextFragment(text_box);
}

}  // namespace blink
