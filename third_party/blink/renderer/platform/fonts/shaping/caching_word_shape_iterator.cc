// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/caching_word_shape_iterator.h"

#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_cache.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"

namespace blink {

const ShapeResult* CachingWordShapeIterator::ShapeWordWithoutSpacing(
    const TextRun& word_run,
    const Font* font) {
  ShapeCacheEntry* cache_entry = font->GetShapeCache()->Add(word_run);
  if (cache_entry && *cache_entry)
    return *cache_entry;

  HarfBuzzShaper shaper(word_run.NormalizedUTF16());
  ShapeResult* shape_result = shaper.Shape(font, word_run.Direction());
  if (!shape_result)
    return nullptr;

  if (cache_entry)
    *cache_entry = shape_result;

  return shape_result;
}

}  // namespace blink
