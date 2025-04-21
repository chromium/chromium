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
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

// A array of glyph offsets. If all offsets are zero, we don't allocate
// storage for reducing memory usage.
class PLATFORM_EXPORT GlyphOffsetArray final {
  DISALLOW_NEW();

 public:
  explicit GlyphOffsetArray(unsigned size) : size_(size) {}

  GlyphOffsetArray(const GlyphOffsetArray& other)
      : storage_(other.storage_), size_(other.size_) {}

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
    return HasStorage() ? size() * sizeof(GlyphOffset) : 0;
  }

  void CopyFrom(const GlyphOffsetArray& other1,
                const GlyphOffsetArray& other2) {
    SECURITY_CHECK(size() == other1.size() + other2.size());
    DCHECK(!other1.IsEmpty());
    DCHECK(!other2.IsEmpty());
    if (other1.HasStorage()) {
      AllocateStorageIfNeeded();
      std::copy(other1.GetStorage(), other1.GetStorage() + other1.size(),
                GetStorage());
    }
    if (other2.HasStorage()) {
      AllocateStorageIfNeeded();
      std::copy(other2.GetStorage(), other2.GetStorage() + other2.size(),
                GetStorage() + other1.size());
    }
  }

  void CopyFromRange(const GlyphDataRange& range) {
    CHECK_EQ(range.size(), size());
    if (!range.HasOffsets() || range.IsEmpty()) {
      storage_.clear();
      return;
    }
    AllocateStorage();
    std::ranges::copy(range.Offsets(), GetStorage());
  }

  GlyphOffset* GetStorage() { return storage_.data(); }
  const GlyphOffset* GetStorage() const { return storage_.data(); }
  bool HasStorage() const { return !storage_.empty(); }

  void Reverse() { storage_.Reverse(); }

  void Shrink(unsigned new_size) {
    DCHECK_GE(new_size, 1u);
    // Note: To follow Vector<T>::Shrink(), we accept |new_size == size()|
    if (new_size == size()) {
      return;
    }
    CHECK_LT(new_size, size());
    size_ = new_size;
    if (!HasStorage()) {
      return;
    }
    storage_.Shrink(new_size);
  }

  // Functions to change one element.
  void AddHeightAt(unsigned index, float delta) {
    CHECK_LT(index, size());
    DCHECK_NE(delta, 0.0f);
    AllocateStorageIfNeeded();
    storage_[index].set_y(storage_[index].y() + delta);
  }

  void AddWidthAt(unsigned index, float delta) {
    CHECK_LT(index, size());
    DCHECK_NE(delta, 0.0f);
    AllocateStorageIfNeeded();
    storage_[index].set_x(storage_[index].x() + delta);
  }

  void SetAt(unsigned index, GlyphOffset offset) {
    CHECK_LT(index, size());
    if (!HasStorage()) {
      if (offset.IsZero()) {
        return;
      }
      AllocateStorage();
    }
    storage_[index] = offset;
  }

  void Trace(Visitor* visitor) const { visitor->Trace(storage_); }

 private:
  // Note: HarfBuzzShaperTest.ShapeVerticalUpright uses non-zero glyph offset.
  void AllocateStorage() {
    DCHECK_GE(size(), 1u);
    DCHECK(!HasStorage());
    storage_.resize(size_);
  }

  void AllocateStorageIfNeeded() {
    if (!HasStorage()) {
      AllocateStorage();
    }
  }

  HeapVector<GlyphOffset> storage_;
  unsigned size_;
};

// For non-zero glyph offset array
template <>
struct GlyphOffsetArray::iterator<true> final {
  STACK_ALLOCATED();

 public:
  // The constructor for ShapeResult
  explicit iterator(const GlyphOffsetArray& array)
      : pointer(array.GetStorage()) {
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
