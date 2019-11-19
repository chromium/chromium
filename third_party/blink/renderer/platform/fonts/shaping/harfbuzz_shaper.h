/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_SHAPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_SHAPER_H_

#include "third_party/blink/renderer/platform/fonts/shaping/run_segmenter.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Font;
class SimpleFontData;
class HarfBuzzShaper;
struct ReshapeQueueItem;
struct RangeData;
struct BufferSlice;

class PLATFORM_EXPORT HarfBuzzShaper final {
  DISALLOW_NEW();

 public:
  HarfBuzzShaper(const String& text) : text_(text) {}

  // Shape a range, defined by the start and end parameters, of the string
  // supplied to the constructor.
  // The start and end positions should represent boundaries where a break may
  // occur, such as at the beginning or end of lines or at element boundaries.
  // If given arbitrary positions the results are not guaranteed to be correct.
  // May be called multiple times; font and direction may vary between calls.
  scoped_refptr<ShapeResult> Shape(const Font*,
                                   TextDirection,
                                   unsigned start,
                                   unsigned end) const;

  // Shape a range that has already been pre-segmented. Start and end positions
  // must match the positions defined by the ranges and must be at valid break
  // positions.
  scoped_refptr<ShapeResult> Shape(
      const Font*,
      TextDirection,
      unsigned start,
      unsigned end,
      const Vector<RunSegmenter::RunSegmenterRange>&) const;

  // Shape a single range. Start and end positions defined by the range.
  scoped_refptr<ShapeResult> Shape(const Font*,
                                   TextDirection,
                                   unsigned start,
                                   unsigned end,
                                   const RunSegmenter::RunSegmenterRange) const;

  // Shape the entire string with a single font and direction.
  // Equivalent to calling the range version with a start offset of zero and an
  // end offset equal to the length.
  scoped_refptr<ShapeResult> Shape(const Font*, TextDirection) const;

  const String& GetText() const { return text_; }
  unsigned TextLength() const { return text_.length(); }

  ~HarfBuzzShaper() = default;

 private:
  // Shapes a single seqment, as identified by the RunSegmenterRange parameter,
  // one or more times taking font fallback into account. The start and end
  // parameters are for the entire text run, not the segment, and are used to
  // determine pre- and post-context for shaping.
  void ShapeSegment(RangeData*,
                    const RunSegmenter::RunSegmenterRange&,
                    ShapeResult*) const;

  void ExtractShapeResults(RangeData*,
                           bool& font_cycle_queued,
                           const ReshapeQueueItem&,
                           const SimpleFontData*,
                           UScriptCode,
                           CanvasRotationInVertical,
                           bool is_last_resort,
                           ShapeResult*) const;

  bool CollectFallbackHintChars(const Deque<ReshapeQueueItem>&,
                                bool needs_hint_list,
                                Vector<UChar32>& hint) const;

  void CommitGlyphs(RangeData*,
                    const SimpleFontData* current_font,
                    UScriptCode current_run_script,
                    CanvasRotationInVertical,
                    bool is_last_resort,
                    const BufferSlice&,
                    ShapeResult*) const;

  const String text_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_SHAPER_H_
