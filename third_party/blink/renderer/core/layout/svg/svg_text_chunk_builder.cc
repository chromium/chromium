/*
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

#include "third_party/blink/renderer/core/layout/svg/svg_text_chunk_builder.h"

#include "third_party/blink/renderer/core/layout/api/line_layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/line/svg_inline_text_box.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg/svg_text_content_element.h"

namespace blink {

float CalculateTextAnchorShift(const ComputedStyle& style, float length) {
  bool is_ltr = style.IsLeftToRightDirection();
  switch (style.TextAnchor()) {
    default:
      NOTREACHED();
      [[fallthrough]];
    case ETextAnchor::kStart:
      return is_ltr ? 0 : -length;
    case ETextAnchor::kMiddle:
      return -length / 2;
    case ETextAnchor::kEnd:
      return is_ltr ? -length : 0;
  }
}

namespace {

bool NeedsTextAnchorAdjustment(const ComputedStyle& style) {
  bool is_ltr = style.IsLeftToRightDirection();
  switch (style.TextAnchor()) {
    default:
      NOTREACHED();
      [[fallthrough]];
    case ETextAnchor::kStart:
      return !is_ltr;
    case ETextAnchor::kMiddle:
      return true;
    case ETextAnchor::kEnd:
      return is_ltr;
  }
}

class ChunkLengthAccumulator {
 public:
  ChunkLengthAccumulator(bool is_vertical)
      : num_characters_(0), length_(0), is_vertical_(is_vertical) {}

  typedef HeapVector<Member<SVGInlineTextBox>>::const_iterator
      BoxListConstIterator;

  void ProcessRange(BoxListConstIterator box_start,
                    BoxListConstIterator box_end);
  void Reset() {
    num_characters_ = 0;
    length_ = 0;
  }

  float length() const { return length_; }
  unsigned NumCharacters() const { return num_characters_; }

 private:
  unsigned num_characters_;
  float length_;
  const bool is_vertical_;
};

void ChunkLengthAccumulator::ProcessRange(BoxListConstIterator box_start,
                                          BoxListConstIterator box_end) {
  SVGTextFragment* last_fragment = nullptr;
  for (auto* box_iter = box_start; box_iter != box_end; ++box_iter) {
    for (SVGTextFragment& fragment : (*box_iter)->TextFragments()) {
      num_characters_ += fragment.length;

      if (is_vertical_)
        length_ += fragment.height;
      else
        length_ += fragment.width;

      if (!last_fragment) {
        last_fragment = &fragment;
        continue;
      }

      // Respect gap between chunks.
      if (is_vertical_)
        length_ += fragment.y - (last_fragment->y + last_fragment->height);
      else
        length_ += fragment.x - (last_fragment->x + last_fragment->width);

      last_fragment = &fragment;
    }
  }
}

}  // namespace

SVGTextChunkBuilder::SVGTextChunkBuilder() = default;

void SVGTextChunkBuilder::ProcessTextChunks(
    const HeapVector<Member<SVGInlineTextBox>>& line_layout_boxes) {
  if (line_layout_boxes.empty())
    return;

  bool found_start = false;
  auto const* box_iter = line_layout_boxes.begin();
  auto const* end_box = line_layout_boxes.end();
  auto const* chunk_start_box = box_iter;
  for (; box_iter != end_box; ++box_iter) {
    if (!(*box_iter)->StartsNewTextChunk())
      continue;

    if (!found_start) {
      found_start = true;
    } else {
      DCHECK_NE(box_iter, chunk_start_box);
      HandleTextChunk(chunk_start_box, box_iter);
    }
    chunk_start_box = box_iter;
  }

  if (!found_start)
    return;

  if (box_iter != chunk_start_box)
    HandleTextChunk(chunk_start_box, box_iter);
}

SVGTextPathChunkBuilder::SVGTextPathChunkBuilder()
    : SVGTextChunkBuilder(), total_length_(0), total_characters_(0) {}

void SVGTextPathChunkBuilder::HandleTextChunk(BoxListConstIterator box_start,
                                              BoxListConstIterator box_end) {
  const ComputedStyle& style = (*box_start)->GetLineLayoutItem().StyleRef();

  ChunkLengthAccumulator length_accumulator(!style.IsHorizontalWritingMode());
  length_accumulator.ProcessRange(box_start, box_end);

  total_length_ += length_accumulator.length();
  total_characters_ += length_accumulator.NumCharacters();
}

static float ComputeTextLengthBias(const SVGTextFragment& fragment,
                                   float scale) {
  float initial_position = fragment.is_vertical ? fragment.y : fragment.x;
  return initial_position + scale * -initial_position;
}

void SVGTextChunkBuilder::HandleTextChunk(BoxListConstIterator box_start,
                                          BoxListConstIterator box_end) {
  DCHECK(*box_start);

  const LineLayoutSVGInlineText text_line_layout =
      LineLayoutSVGInlineText((*box_start)->GetLineLayoutItem());
  const ComputedStyle& style = text_line_layout.StyleRef();

  // Handle 'lengthAdjust' property.
  float desired_text_length = 0;
  SVGLengthAdjustType length_adjust = kSVGLengthAdjustUnknown;
  if (SVGTextContentElement* text_content_element =
          SVGTextContentElement::ElementFromLineLayoutItem(
              text_line_layout.Parent())) {
    length_adjust = text_content_element->lengthAdjust()->CurrentEnumValue();

    SVGLengthContext length_context(text_content_element);
    if (text_content_element->TextLengthIsSpecifiedByUser())
      desired_text_length =
          text_content_element->textLength()->CurrentValue()->Value(
              length_context);
    else
      desired_text_length = 0;
  }

  bool process_text_length = desired_text_length > 0;
  bool process_text_anchor = NeedsTextAnchorAdjustment(style);
  if (!process_text_anchor && !process_text_length)
    return;

  bool is_vertical_text = !style.IsHorizontalWritingMode();

  // Calculate absolute length of whole text chunk (starting from text box
  // 'start', spanning 'length' text boxes).
  ChunkLengthAccumulator length_accumulator(is_vertical_text);
  length_accumulator.ProcessRange(box_start, box_end);

  if (process_text_length) {
    float chunk_length = length_accumulator.length();
    if (length_adjust == kSVGLengthAdjustSpacing) {
      float text_length_shift = 0;
      if (length_accumulator.NumCharacters() > 1) {
        text_length_shift = desired_text_length - chunk_length;
        text_length_shift /= length_accumulator.NumCharacters() - 1;
      }
      unsigned at_character = 0;
      for (auto* box_iter = box_start; box_iter != box_end; ++box_iter) {
        Vector<SVGTextFragment>& fragments = (*box_iter)->TextFragments();
        if (fragments.empty())
          continue;
        ProcessTextLengthSpacingCorrection(is_vertical_text, text_length_shift,
                                           fragments, at_character);
      }

      // Fragments have been adjusted, we have to recalculate the chunk
      // length, to be able to apply the text-anchor shift.
      if (process_text_anchor) {
        length_accumulator.Reset();
        length_accumulator.ProcessRange(box_start, box_end);
      }
    } else {
      DCHECK_EQ(length_adjust, kSVGLengthAdjustSpacingAndGlyphs);
      float text_length_scale = desired_text_length / chunk_length;
      float text_length_bias = 0;

      bool found_first_fragment = false;
      for (auto* box_iter = box_start; box_iter != box_end; ++box_iter) {
        SVGInlineTextBox* text_box = *box_iter;
        Vector<SVGTextFragment>& fragments = text_box->TextFragments();
        if (fragments.empty())
          continue;

        if (!found_first_fragment) {
          found_first_fragment = true;
          text_length_bias =
              ComputeTextLengthBias(fragments.front(), text_length_scale);
        }

        ApplyTextLengthScaleAdjustment(text_length_scale, text_length_bias,
                                       fragments);
      }
    }
  }

  if (!process_text_anchor)
    return;

  float text_anchor_shift =
      CalculateTextAnchorShift(style, length_accumulator.length());
  for (auto* box_iter = box_start; box_iter != box_end; ++box_iter) {
    Vector<SVGTextFragment>& fragments = (*box_iter)->TextFragments();
    if (fragments.empty())
      continue;
    ProcessTextAnchorCorrection(is_vertical_text, text_anchor_shift, fragments);
  }
}

void SVGTextChunkBuilder::ProcessTextLengthSpacingCorrection(
    bool is_vertical_text,
    float text_length_shift,
    Vector<SVGTextFragment>& fragments,
    unsigned& at_character) {
  for (SVGTextFragment& fragment : fragments) {
    if (is_vertical_text)
      fragment.y += text_length_shift * at_character;
    else
      fragment.x += text_length_shift * at_character;

    at_character += fragment.length;
  }
}

void SVGTextChunkBuilder::ApplyTextLengthScaleAdjustment(
    float text_length_scale,
    float text_length_bias,
    Vector<SVGTextFragment>& fragments) {
  for (SVGTextFragment& fragment : fragments) {
    DCHECK_EQ(fragment.length_adjust_scale, 1u);
    fragment.length_adjust_scale = text_length_scale;
    fragment.length_adjust_bias = text_length_bias;
  }
}

void SVGTextChunkBuilder::ProcessTextAnchorCorrection(
    bool is_vertical_text,
    float text_anchor_shift,
    Vector<SVGTextFragment>& fragments) {
  for (SVGTextFragment& fragment : fragments) {
    if (is_vertical_text)
      fragment.y += text_anchor_shift;
    else
      fragment.x += text_anchor_shift;
  }
}

}  // namespace blink
