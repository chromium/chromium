// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_GLYPH_DATA_RANGE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_GLYPH_DATA_RANGE_H_

#include <iterator>

#include "third_party/blink/renderer/platform/fonts/shaping/glyph_data.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

struct ShapeResultRun;

// Represents an indexed range of HarfBuzzRunGlyphData within a ShapeResultRun.
class PLATFORM_EXPORT GlyphDataRange {
  DISALLOW_NEW();

 public:
  GlyphDataRange() = default;
  explicit GlyphDataRange(const ShapeResultRun&);

  unsigned size() const { return size_; }
  bool IsEmpty() const { return !size_; }
  const ShapeResultRun* GetRun() const { return run_.Get(); }

  struct NonCompactGlyphPointerRange {
    const HarfBuzzRunGlyphData* begin;
    const HarfBuzzRunGlyphData* end;
  };
  NonCompactGlyphPointerRange NonCompactGlyphPointers() const;

  bool HasOffsets() const;

  // The `span` of `GlyphOffset` if `HasOffsets()`, or an empty span.
  base::span<const GlyphOffset> Offsets() const;

  // `dest` must be exactly `size()` long.
  void ExpandInto(base::span<HarfBuzzRunGlyphData> dest) const;

  void Trace(Visitor*) const;

  GlyphDataRange FindGlyphDataRange(bool is_rtl,
                                    unsigned start_character_index,
                                    unsigned end_character_index) const;

  class Reader {
    STACK_ALLOCATED();

   public:
    class Iterator {
      STACK_ALLOCATED();

     public:
      using iterator_category = std::bidirectional_iterator_tag;
      using iterator_concept = std::bidirectional_iterator_tag;
      using value_type = HarfBuzzRunGlyphData;
      using difference_type = std::ptrdiff_t;
      using pointer = const HarfBuzzRunGlyphData*;
      using reference = const HarfBuzzRunGlyphData&;

      Iterator() = default;

      const HarfBuzzRunGlyphData& operator*() const {
        return (*reader_)[index_];
      }
      Iterator& operator++() {
        ++index_;
        return *this;
      }
      Iterator operator++(int) {
        Iterator previous = *this;
        ++*this;
        return previous;
      }
      Iterator& operator--() {
        --index_;
        return *this;
      }
      Iterator operator--(int) {
        Iterator previous = *this;
        --*this;
        return previous;
      }
      bool operator==(const Iterator&) const = default;

     private:
      friend class Reader;

      Iterator(const Reader* reader, unsigned index)
          : reader_(reader), index_(index) {}

      const Reader* reader_ = nullptr;
      unsigned index_ = 0;
    };

    explicit Reader(const GlyphDataRange& range);
    explicit Reader(const ShapeResultRun& run);

    Iterator begin() const { return Iterator(this, 0); }
    Iterator end() const { return Iterator(this, size()); }

    unsigned size() const { return static_cast<unsigned>(glyphs_.size()); }
    const HarfBuzzRunGlyphData& operator[](unsigned index) const {
      return glyphs_[index];
    }

   private:
    base::span<const HarfBuzzRunGlyphData> glyphs_;
  };

 private:
  GlyphDataRange(const ShapeResultRun* run, wtf_size_t index, wtf_size_t size)
      : run_(run), index_(index), size_(size) {}

  Member<const ShapeResultRun> run_;
  wtf_size_t index_ = 0;
  wtf_size_t size_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_GLYPH_DATA_RANGE_H_
