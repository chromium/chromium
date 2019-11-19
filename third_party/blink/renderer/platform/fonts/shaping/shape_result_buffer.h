// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_BUFFER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct CharacterRange;
class FontDescription;
struct GlyphData;
class ShapeResultBloberizer;
class TextRun;

class PLATFORM_EXPORT ShapeResultBuffer {
  STACK_ALLOCATED();

 public:
  ShapeResultBuffer() : has_vertical_offsets_(false) {}

  void AppendResult(scoped_refptr<const ShapeResult> result) {
    has_vertical_offsets_ |= result->HasVerticalOffsets();
    results_.push_back(std::move(result));
  }

  bool HasVerticalOffsets() const { return has_vertical_offsets_; }

  int OffsetForPosition(const TextRun& run,
                        float target_x,
                        IncludePartialGlyphsOption,
                        BreakGlyphsOption) const;
  CharacterRange GetCharacterRange(const StringView& text,
                                   TextDirection,
                                   float total_width,
                                   unsigned from,
                                   unsigned to) const;
  Vector<CharacterRange> IndividualCharacterRanges(TextDirection,
                                                   float total_width) const;
  Vector<double> IndividualCharacterAdvances(const StringView&,
                                             TextDirection,
                                             float total_width) const;

  Vector<ShapeResult::RunFontData> GetRunFontData() const;

  GlyphData EmphasisMarkGlyphData(const FontDescription&) const;

  void ExpandRangeToIncludePartialGlyphs(int* from, int* to) const;

 private:
  friend class ShapeResultBloberizer;

  static void AddRunInfoAdvances(const ShapeResult::RunInfo& run_info,
                                 double offset,
                                 Vector<double>& advances);

  // Empirically, cases where we get more than 50 ShapeResults are extremely
  // rare.
  Vector<scoped_refptr<const ShapeResult>, 64> results_;
  bool has_vertical_offsets_;

  DISALLOW_COPY_AND_ASSIGN(ShapeResultBuffer);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_BUFFER_H_
