// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_GLYPH_OFFSET_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_GLYPH_OFFSET_ITERATOR_H_

#include "third_party/blink/renderer/platform/fonts/shaping/glyph_data.h"
#include "third_party/blink/renderer/platform/fonts/shaping/glyph_data_range.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

// An iterator for `ShapeResultRun::offsets_`.
//
// Since it could be empty if there are no glyph offsets in the run, this
// iterator makes iterating offsets to be no-operations in such case.
template <bool has_non_zero_glyph_offsets>
struct PLATFORM_EXPORT GlyphOffsetIterator final {};

// For non-zero glyph offset array
template <>
struct GlyphOffsetIterator<true> final {
  STACK_ALLOCATED();

 public:
  explicit GlyphOffsetIterator(base::span<const GlyphOffset> offsets)
      : iterator_(offsets.begin()) {
    // An empty span should use `has_non_zero_glyph_offsets = false`.
    DCHECK(!offsets.empty());
  }

  // The constructor for ShapeResultView
  explicit GlyphOffsetIterator(const GlyphDataRange& range)
      : GlyphOffsetIterator(range.Offsets()) {}

  GlyphOffset operator*() const { return *iterator_; }
  void operator++() { ++iterator_; }
  void operator+=(ptrdiff_t s) { iterator_ += s; }

  GlyphOffset operator[](size_t i) const { return *(iterator_ + i); }

 private:
  base::span<const GlyphOffset>::iterator iterator_;
};

// For zero glyph offset array
template <>
struct GlyphOffsetIterator<false> final {
  explicit GlyphOffsetIterator(base::span<const GlyphOffset> offsets) {
    // An empty span should use `has_non_zero_glyph_offsets = false`.
    DCHECK(offsets.empty());
  }

  explicit GlyphOffsetIterator(const GlyphDataRange& range) {
    DCHECK(!range.HasOffsets());
  }

  GlyphOffset operator*() const { return GlyphOffset(); }
  void operator++() {}
  void operator+=(ptrdiff_t) {}
  GlyphOffset operator[](size_t) const { return GlyphOffset(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_GLYPH_OFFSET_ITERATOR_H_
