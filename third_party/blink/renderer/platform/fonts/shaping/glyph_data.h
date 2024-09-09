// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_GLYPH_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_GLYPH_DATA_H_

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

// Because glyph offsets are often zero, particularly for Latin runs, we hold it
// in |ShapeResult::RunInfo::GlyphDataCollection::offsets_| for reducing memory
// usage.
struct HarfBuzzRunGlyphData {
  DISALLOW_NEW();

  // The max number of characters in a |RunInfo| is limited by
  // |character_index|.
  static constexpr unsigned kCharacterIndexBits = 15;
  static constexpr unsigned kMaxCharacters = 1 << kCharacterIndexBits;
  static constexpr unsigned kMaxCharacterIndex = kMaxCharacters - 1;
  // The max number of glyphs in a |RunInfo|. This make the number
  // of glyphs predictable and minimizes the buffer reallocations.
  static constexpr unsigned kMaxGlyphs = kMaxCharacters;

  HarfBuzzRunGlyphData() = default;
  HarfBuzzRunGlyphData(unsigned glyph,
                       unsigned character_index,
                       bool safe_to_break_before,
                       TextRunLayoutUnit advance)
      : glyph(glyph),
        character_index(character_index),
        safe_to_break_before(safe_to_break_before),
        advance(advance) {}

  void SetAdvance(TextRunLayoutUnit value) { advance = value; }
  void AddAdvance(TextRunLayoutUnit value) { advance += value; }
  void SetAdvance(float value) {
    advance = TextRunLayoutUnit::FromFloatRound(value);
  }
  void AddAdvance(float value) {
    advance += TextRunLayoutUnit::FromFloatRound(value);
  }

  unsigned glyph : 16;
  // The index of the character this glyph is for. To use as an index of
  // |String|, it is the index of UTF16 code unit, and it is always at the
  // HarfBuzz cluster boundary.
  unsigned character_index : kCharacterIndexBits;
  unsigned safe_to_break_before : 1;

  TextRunLayoutUnit advance;
};
static_assert(std::is_trivially_copyable<HarfBuzzRunGlyphData>::value, "");

using GlyphOffset = gfx::Vector2dF;

// Represents a range of HarfBuzzRunGlyphData. |begin| and |end| follow the
// iterator pattern; i.e., |begin| is lower or equal to |end| in the address
// space regardless of LTR/RTL. |begin| is inclusive, |end| is exclusive.
struct GlyphDataRange {
  GlyphDataRange FindGlyphDataRange(bool is_rtl,
                                    unsigned start_character_index,
                                    unsigned end_character_index) const;
  unsigned size() const { return static_cast<unsigned>(end - begin); }

  const HarfBuzzRunGlyphData* begin = nullptr;
  const HarfBuzzRunGlyphData* end = nullptr;
  const GlyphOffset* offsets = nullptr;
};

// Find the range of HarfBuzzRunGlyphData for the specified character index
// range. This function uses binary search twice, hence O(2 log n).
inline GlyphDataRange GlyphDataRange::FindGlyphDataRange(
    bool is_rtl,
    unsigned start_character_index,
    unsigned end_character_index) const {
  const auto comparer = [](const HarfBuzzRunGlyphData& glyph_data,
                           unsigned index) {
    return glyph_data.character_index < index;
  };
  if (!is_rtl) {
    const HarfBuzzRunGlyphData* start_glyph =
        std::lower_bound(begin, end, start_character_index, comparer);
    if (start_glyph == end) [[unlikely]] {
      return GlyphDataRange();
    }
    const HarfBuzzRunGlyphData* end_glyph =
        std::lower_bound(start_glyph, end, end_character_index, comparer);
    if (offsets) {
      return {start_glyph, end_glyph, offsets + (start_glyph - begin)};
    }
    return {start_glyph, end_glyph, nullptr};
  }

  // RTL needs to use reverse iterators because there maybe multiple glyphs
  // for a character, and we want to find the first one in the logical order.
  const auto rbegin = std::reverse_iterator<const HarfBuzzRunGlyphData*>(end);
  const auto rend = std::reverse_iterator<const HarfBuzzRunGlyphData*>(begin);
  const auto start_glyph_it =
      std::lower_bound(rbegin, rend, start_character_index, comparer);
  if (start_glyph_it == rend) [[unlikely]] {
    return GlyphDataRange();
  }
  const auto end_glyph_it =
      std::lower_bound(start_glyph_it, rend, end_character_index, comparer);
  // Convert reverse iterators to pointers. Then increment to make |begin|
  // inclusive and |end| exclusive.
  const HarfBuzzRunGlyphData* start_glyph = &*end_glyph_it + 1;
  const HarfBuzzRunGlyphData* end_glyph = &*start_glyph_it + 1;
  if (offsets) {
    return {start_glyph, end_glyph, offsets + (start_glyph - begin)};
  }
  return {start_glyph, end_glyph, nullptr};
}

// A array of glyph offsets. If all offsets are zero, we don't allocate
// storage for reducing memory usage.
class GlyphOffsetArray final {
 public:
  explicit GlyphOffsetArray(unsigned size) : size_(size) {}

  GlyphOffsetArray(const GlyphOffsetArray& other) : size_(other.size_) {
    if (!other.storage_) {
      return;
    }
    std::copy(other.storage_.get(), other.storage_.get() + other.size(),
              AllocateStorage());
  }

  // A return value of |GetOffsets()| to represent optional |GlyphOffset|
  // array.
  template <bool has_non_zero_glyph_offsets>
  struct iterator final {};

  template <bool has_non_zero_glyph_offsets>
  iterator<has_non_zero_glyph_offsets> GetIterator() const {
    return iterator<has_non_zero_glyph_offsets>(*this);
  }

  template <bool has_non_zero_glyph_offsets>
  iterator<has_non_zero_glyph_offsets> GetIteratorForRange(
      const GlyphDataRange& range) const {
    return iterator<has_non_zero_glyph_offsets>(range);
  }

  unsigned size() const { return size_; }
  bool IsEmpty() const { return size() == 0; }

  size_t ByteSize() const {
    return storage_ ? size() * sizeof(GlyphOffset) : 0;
  }

  void CopyFrom(const GlyphOffsetArray& other1,
                const GlyphOffsetArray& other2) {
    SECURITY_CHECK(size() == other1.size() + other2.size());
    DCHECK(!other1.IsEmpty());
    DCHECK(!other2.IsEmpty());
    if (other1.storage_) {
      if (!storage_) {
        AllocateStorage();
      }
      std::copy(other1.storage_.get(), other1.storage_.get() + other1.size(),
                storage_.get());
    }
    if (other2.storage_) {
      if (!storage_) {
        AllocateStorage();
      }
      std::copy(other2.storage_.get(), other2.storage_.get() + other2.size(),
                storage_.get() + other1.size());
    }
  }

  void CopyFromRange(const GlyphDataRange& range) {
    CHECK_EQ(range.size(), size());
    if (!range.offsets || range.size() == 0) {
      storage_.reset();
      return;
    }
    std::copy(range.offsets, range.offsets + range.size(), AllocateStorage());
  }

  GlyphOffset* GetStorage() const { return storage_.get(); }
  bool HasStorage() const { return storage_.get(); }

  void Reverse() {
    if (!storage_) {
      return;
    }
    std::reverse(storage_.get(), storage_.get() + size());
  }

  void Shrink(unsigned new_size) {
    DCHECK_GE(new_size, 1u);
    // Note: To follow Vector<T>::Shrink(), we accept |new_size == size()|
    if (new_size == size()) {
      return;
    }
    CHECK_LT(new_size, size());
    size_ = new_size;
    if (!storage_) {
      return;
    }
    GlyphOffset* new_offsets = new GlyphOffset[new_size];
    std::copy(storage_.get(), storage_.get() + new_size, new_offsets);
    storage_.reset(new_offsets);
  }

  // Functions to change one element.
  void AddHeightAt(unsigned index, float delta) {
    CHECK_LT(index, size());
    DCHECK_NE(delta, 0.0f);
    if (!storage_) {
      AllocateStorage();
    }
    storage_[index].set_y(storage_[index].y() + delta);
  }

  void AddWidthAt(unsigned index, float delta) {
    CHECK_LT(index, size());
    DCHECK_NE(delta, 0.0f);
    if (!storage_) {
      AllocateStorage();
    }
    storage_[index].set_x(storage_[index].x() + delta);
  }

  void SetAt(unsigned index, GlyphOffset offset) {
    CHECK_LT(index, size());
    if (!storage_) {
      if (offset.IsZero()) {
        return;
      }
      AllocateStorage();
    }
    storage_[index] = offset;
  }

 private:
  // Note: HarfBuzzShaperTest.ShapeVerticalUpright uses non-zero glyph offset.
  GlyphOffset* AllocateStorage() {
    DCHECK_GE(size(), 1u);
    DCHECK(!storage_);
    storage_.reset(new GlyphOffset[size()]);
    return storage_.get();
  }

  std::unique_ptr<GlyphOffset[]> storage_;
  unsigned size_;
};

// For non-zero glyph offset array
template <>
struct GlyphOffsetArray::iterator<true> final {
  // The constructor for ShapeResult
  explicit iterator(const GlyphOffsetArray& array)
      : pointer(array.storage_.get()) {
    DCHECK(pointer);
  }

  // The constructor for ShapeResultView
  explicit iterator(const GlyphDataRange& range) : pointer(range.offsets) {
    DCHECK(pointer);
  }

  GlyphOffset operator*() const { return *pointer; }
  void operator++() { ++pointer; }

  const GlyphOffset* pointer;
};

// For zero glyph offset array
template <>
struct GlyphOffsetArray::iterator<false> final {
  explicit iterator(const GlyphOffsetArray& array) {
    DCHECK(!array.HasStorage());
  }
  explicit iterator(const GlyphDataRange& range) { DCHECK(!range.offsets); }
  GlyphOffset operator*() const { return GlyphOffset(); }
  void operator++() {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_GLYPH_DATA_H_
