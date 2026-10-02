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

  wtf_size_t size() const { return size_; }
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
                                    wtf_size_t start_character_index,
                                    wtf_size_t end_character_index) const;

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
      using reference = HarfBuzzRunGlyphData;

      Iterator() = default;

      HarfBuzzRunGlyphData operator*() const { return (*reader_)[index_]; }
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

      Iterator(const Reader* reader, wtf_size_t index)
          : reader_(reader), index_(index) {}

      const Reader* reader_ = nullptr;
      wtf_size_t index_ = 0;
    };

    explicit Reader(const GlyphDataRange& range);
    explicit Reader(const ShapeResultRun& run);

    Iterator begin() const { return Iterator(this, 0); }
    Iterator end() const { return Iterator(this, size()); }

    wtf_size_t size() const {
      return static_cast<wtf_size_t>(
          compact_glyphs_.empty() ? glyphs_.size() : compact_glyphs_.size());
    }

    HarfBuzzRunGlyphData operator[](wtf_size_t index) const {
      if (!compact_glyphs_.empty()) [[unlikely]] {
        const base::span<const uint16_t> compact_glyphs = compact_glyphs_;
        return HarfBuzzRunGlyphData(compact_glyphs[index],
                                    compact_index_offset_ + index,
                                    SafeToBreak::kSafe, compact_advance_);
      }
      return glyphs_[index];
    }

   private:
    void Init(const ShapeResultRun& run, unsigned index, unsigned size);

    base::span<const HarfBuzzRunGlyphData> glyphs_;
    base::span<const uint16_t> compact_glyphs_;
    TextRunLayoutUnit compact_advance_;
    unsigned compact_index_offset_ = 0;
  };

  HarfBuzzRunGlyphData GlyphAtForTest(unsigned index) const;
  bool IsCompactSource() const;
  TextRunLayoutUnit CompactSourceAdvance() const;

 private:
  GlyphDataRange FindCompactGlyphDataRange(unsigned start_character_index,
                                           unsigned end_character_index) const;

  GlyphDataRange(const ShapeResultRun* run, wtf_size_t index, wtf_size_t size)
      : run_(run), index_(index), size_(size) {}

  Member<const ShapeResultRun> run_;
  wtf_size_t index_ = 0;
  wtf_size_t size_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_GLYPH_DATA_RANGE_H_
