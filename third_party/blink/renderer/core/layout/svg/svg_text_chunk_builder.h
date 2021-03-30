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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_TEXT_CHUNK_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_TEXT_CHUNK_BUILDER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ComputedStyle;
class SVGInlineTextBox;
struct SVGTextFragment;

// SVGTextChunkBuilder performs the third layout phase for SVG text.
//
// Phase one built the layout information from the SVG DOM stored in the
// LayoutSVGInlineText objects (SVGTextLayoutAttributes).
// Phase two performed the actual per-character layout, computing the final
// positions for each character, stored in the SVGInlineTextBox objects
// (SVGTextFragment).
// Phase three performs all modifications that have to be applied to each
// individual text chunk (text-anchor & textLength).

class SVGTextChunkBuilder {
  STACK_ALLOCATED();

 public:
  SVGTextChunkBuilder();
  SVGTextChunkBuilder(const SVGTextChunkBuilder&) = delete;
  SVGTextChunkBuilder& operator=(const SVGTextChunkBuilder&) = delete;

  void ProcessTextChunks(const Vector<SVGInlineTextBox*>&);

 protected:
  typedef Vector<SVGInlineTextBox*>::const_iterator BoxListConstIterator;

  virtual void HandleTextChunk(BoxListConstIterator box_start,
                               BoxListConstIterator box_end);

 private:
  void ProcessTextLengthSpacingCorrection(bool is_vertical_text,
                                          float text_length_shift,
                                          Vector<SVGTextFragment>&,
                                          unsigned& at_character);
  void ApplyTextLengthScaleAdjustment(float text_length_scale,
                                      float text_length_bias,
                                      Vector<SVGTextFragment>&);
  void ProcessTextAnchorCorrection(bool is_vertical_text,
                                   float text_anchor_shift,
                                   Vector<SVGTextFragment>&);
};

class SVGTextPathChunkBuilder final : public SVGTextChunkBuilder {
  STACK_ALLOCATED();

 public:
  SVGTextPathChunkBuilder();
  SVGTextPathChunkBuilder(const SVGTextPathChunkBuilder&) = delete;
  SVGTextPathChunkBuilder& operator=(const SVGTextPathChunkBuilder&) = delete;

  float TotalLength() const { return total_length_; }
  unsigned TotalCharacters() const { return total_characters_; }

 private:
  void HandleTextChunk(BoxListConstIterator box_start,
                       BoxListConstIterator box_end) override;

  float total_length_;
  unsigned total_characters_;
};

// Compute the "shift" induced by the 'text-anchor' property.
float CalculateTextAnchorShift(const ComputedStyle&, float length);

}  // namespace blink

#endif
