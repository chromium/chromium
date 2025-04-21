// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_GLYPH_OFFSET_ARRAY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_GLYPH_OFFSET_ARRAY_H_

#include "third_party/blink/renderer/platform/fonts/shaping/glyph_data.h"
#include "third_party/blink/renderer/platform/fonts/shaping/glyph_data_range.h"

namespace blink {

// A array of glyph offsets. If all offsets are zero, we don't allocate
// storage for reducing memory usage.
class PLATFORM_EXPORT GlyphOffsetArray final {
  DISALLOW_NEW();

 public:
  explicit GlyphOffsetArray(unsigned size) : size_(size) {}

  GlyphOffsetArray(const GlyphOffsetArray& other) : size_(other.size_) {
    if (!other.storage_) {
      return;
    }
    std::copy(other.storage_.get(), other.storage_.get() + other.size(),
              AllocateStorage());
  }

  // The `span` of `GlyphOffset` if `HasStorage()`, or an empty span.
  explicit operator base::span<const GlyphOffset>() const {
    return HasStorage() ? base::span{GetStorage(), size()}
                        : base::span<const GlyphOffset>{};
  }

  // A return value of |GetOffsets()| to represent optional |GlyphOffset|
  // array.
  template <bool has_non_zero_glyph_offsets>
  struct iterator final {};

  template <bool has_non_zero_glyph_offsets>
  iterator<has_non_zero_glyph_offsets> GetIterator() const {
    return iterator<has_non_zero_glyph_offsets>(*this);
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
    if (!range.HasOffsets() || range.IsEmpty()) {
      storage_.reset();
      return;
    }
    std::ranges::copy(range.Offsets(), AllocateStorage());
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
  STACK_ALLOCATED();

 public:
  // The constructor for ShapeResult
  explicit iterator(const GlyphOffsetArray& array)
      : pointer(array.storage_.get()) {
    DCHECK(pointer);
  }

  // The constructor for ShapeResultView
  explicit iterator(const GlyphDataRange& range) : pointer(range.Offset()) {
    DCHECK(pointer);
  }

  GlyphOffset operator*() const { return *pointer; }
  void operator++() { ++pointer; }
  void operator+=(ptrdiff_t s) { pointer += s; }

  GlyphOffset operator[](size_t i) const { return pointer[i]; }

  const GlyphOffset* pointer;
};

// For zero glyph offset array
template <>
struct GlyphOffsetArray::iterator<false> final {
  explicit iterator(const GlyphOffsetArray& array) {
    DCHECK(!array.HasStorage());
  }
  explicit iterator(const GlyphDataRange& range) {
    DCHECK(!range.HasOffsets());
  }

  GlyphOffset operator*() const { return GlyphOffset(); }
  void operator++() {}
  void operator+=(ptrdiff_t) {}
  GlyphOffset operator[](size_t) const { return GlyphOffset(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_GLYPH_OFFSET_ARRAY_H_
