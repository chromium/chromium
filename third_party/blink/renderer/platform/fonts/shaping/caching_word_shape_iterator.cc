// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/caching_word_shape_iterator.h"

#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"

namespace blink {

scoped_refptr<const ShapeResult>
CachingWordShapeIterator::ShapeWordWithoutSpacing(const TextRun& word_run,
                                                  const Font* font) {
  ShapeCacheEntry* cache_entry = shape_cache_->Add(word_run, ShapeCacheEntry());
  if (cache_entry && cache_entry->shape_result_)
    return cache_entry->shape_result_;

  const String word_text = word_run.NormalizedUTF16();
  HarfBuzzShaper shaper(word_text);
  scoped_refptr<const ShapeResult> shape_result =
      shaper.Shape(font, word_run.Direction());
  if (!shape_result)
    return nullptr;

  shape_result->SetDeprecatedInkBounds(shape_result->ComputeInkBounds());
  if (cache_entry)
    cache_entry->shape_result_ = shape_result;

  return shape_result;
}

scoped_refptr<const ShapeResult> CachingWordShapeIterator::ShapeWord(
    const TextRun& word_run,
    const Font* font) {
  scoped_refptr<const ShapeResult> result =
      ShapeWordWithoutSpacing(word_run, font);
  if (LIKELY(!spacing_.HasSpacing()))
    return result;

  scoped_refptr<const ShapeResult> spacing_result =
      result->ApplySpacingToCopy(spacing_, word_run);
  FloatRect ink_bounds = spacing_result->ComputeInkBounds();

  // Return bounds as is because glyph bounding box is in logical space.
  if (spacing_result->Width() >= 0 && ink_bounds.Width() >= 0) {
    spacing_result->SetDeprecatedInkBounds(ink_bounds);
    return spacing_result;
  }

  // Negative word-spacing and/or letter-spacing may cause some glyphs to
  // overflow the left boundary and result negative measured width. Adjust glyph
  // bounds accordingly to cover the overflow.
  // The negative width should be clamped to 0 in CSS box model, but it's up to
  // caller's responsibility.
  float left = std::min(spacing_result->Width(), ink_bounds.Width());
  if (left < ink_bounds.X()) {
    // The right edge should be the width of the first character in most cases,
    // but computing it requires re-measuring bounding box of each glyph. Leave
    // it unchanged, which gives an excessive right edge but assures it covers
    // all glyphs.
    ink_bounds.ShiftXEdgeTo(left);
  } else {
    ink_bounds.SetWidth(ink_bounds.Width());
  }

  spacing_result->SetDeprecatedInkBounds(ink_bounds);
  return spacing_result;
}

}  // namespace blink
