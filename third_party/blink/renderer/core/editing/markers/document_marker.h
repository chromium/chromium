/*
 * This file is part of the DOM implementation for WebCore.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_DOCUMENT_MARKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_DOCUMENT_MARKER_H_

#include <bit>
#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector_traits.h"

namespace blink {

// A range of a node within a document that is "marked", such as the range of a
// misspelled word. It optionally includes a description that could be displayed
// in the user interface.
class CORE_EXPORT DocumentMarker : public GarbageCollected<DocumentMarker> {
 public:
  enum MarkerTypeIndex {
    kSpellingMarkerIndex = 0,
    kGrammarMarkerIndex,
    kTextMatchMarkerIndex,
    kCompositionMarkerIndex,
    kActiveSuggestionMarkerIndex,
    kSuggestionMarkerIndex,
    kTextFragmentMarkerIndex,
    kCustomHighlightMarkerIndex,
    kGlicMarkerIndex,
    kPreviewStylusGestureMarkerIndex,
    kMarkerTypeIndexesCount
  };

  enum MarkerType : uint32_t {
    kSpelling = 1 << kSpellingMarkerIndex,
    kGrammar = 1 << kGrammarMarkerIndex,
    kTextMatch = 1 << kTextMatchMarkerIndex,
    kComposition = 1 << kCompositionMarkerIndex,
    kActiveSuggestion = 1 << kActiveSuggestionMarkerIndex,
    kSuggestion = 1 << kSuggestionMarkerIndex,
    kTextFragment = 1 << kTextFragmentMarkerIndex,
    kCustomHighlight = 1 << kCustomHighlightMarkerIndex,
    kGlic = 1 << kGlicMarkerIndex,
    kPreviewStylusGesture = 1 << kPreviewStylusGestureMarkerIndex,
  };

  class MarkerTypesIterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = MarkerType;
    using difference_type = std::ptrdiff_t;
    using pointer = MarkerType*;
    using reference = MarkerType&;

    explicit MarkerTypesIterator(uint32_t marker_types)
        : remaining_types_(marker_types) {}
    MarkerTypesIterator(const MarkerTypesIterator& other) = default;

    bool operator==(const MarkerTypesIterator& other) const {
      return remaining_types_ == other.remaining_types_;
    }

    MarkerTypesIterator& operator++() {
      DCHECK(remaining_types_);
      // Turn off least significant 1-bit (from Hacker's Delight 2-1)
      // Example:
      // 7: 7 & 6 = 6
      // 6: 6 & 5 = 4
      // 4: 4 & 3 = 0
      remaining_types_ &= (remaining_types_ - 1);
      return *this;
    }

    MarkerType operator*() const {
      DCHECK(remaining_types_);
      // Isolate least significant 1-bit (from Hacker's Delight 2-1)
      // Example:
      // 7: 7 & -7 = 1
      // 6: 6 & -6 = 2
      // 4: 4 & -4 = 4
      return static_cast<MarkerType>(remaining_types_ &
                                     (~remaining_types_ + 1));
    }

   private:
    uint32_t remaining_types_;
  };

  class MarkerTypes {
    DISALLOW_NEW();

   public:
    explicit MarkerTypes(uint32_t mask = 0) : mask_(mask) {}

    static MarkerTypes All() {
      return MarkerTypes((1 << kMarkerTypeIndexesCount) - 1);
    }

    static MarkerTypes AllBut(const MarkerTypes& types) {
      return MarkerTypes(All().mask_ & ~types.mask_);
    }

    static MarkerTypes HighlightPseudos() {
      return MarkerTypes(kTextFragment | kSpelling | kGrammar |
                         kCustomHighlight);
    }

    static MarkerTypes ActiveSuggestion() {
      return MarkerTypes(kActiveSuggestion);
    }
    static MarkerTypes Composition() { return MarkerTypes(kComposition); }
    static MarkerTypes PreviewStylusGesture() {
      return MarkerTypes(kPreviewStylusGesture);
    }
    static MarkerTypes Grammar() { return MarkerTypes(kGrammar); }
    static MarkerTypes Misspelling() {
      return MarkerTypes(kSpelling | kGrammar);
    }
    static MarkerTypes Spelling() { return MarkerTypes(kSpelling); }
    static MarkerTypes TextMatch() { return MarkerTypes(kTextMatch); }
    static MarkerTypes Suggestion() { return MarkerTypes(kSuggestion); }
    static MarkerTypes TextFragment() { return MarkerTypes(kTextFragment); }
    static MarkerTypes CustomHighlight() {
      return MarkerTypes(kCustomHighlight);
    }
    static MarkerTypes Glic() { return MarkerTypes(kGlic); }

    bool Contains(MarkerType type) const { return mask_ & type; }
    bool Intersects(const MarkerTypes& types) const {
      return (mask_ & types.mask_);
    }
    std::optional<MarkerType> IsOneMarkerType() {
      if (std::has_single_bit(mask_)) {
        return static_cast<MarkerType>(mask_);
      }
      return std::nullopt;
    }
    bool operator==(const MarkerTypes& other) const {
      return mask_ == other.mask_;
    }

    MarkerTypes Add(const MarkerTypes& types) const {
      return MarkerTypes(mask_ | types.mask_);
    }

    MarkerTypes Subtract(const MarkerTypes& types) const {
      return MarkerTypes(mask_ & ~types.mask_);
    }

    MarkerTypesIterator begin() const { return MarkerTypesIterator(mask_); }
    MarkerTypesIterator end() const { return MarkerTypesIterator(0); }

   private:
    uint32_t mask_;
  };

  DocumentMarker(const DocumentMarker&) = delete;
  DocumentMarker& operator=(const DocumentMarker&) = delete;
  virtual ~DocumentMarker();

  virtual MarkerType GetType() const = 0;
  wtf_size_t StartOffset() const { return start_offset_; }
  wtf_size_t EndOffset() const { return end_offset_; }

  struct MarkerOffsets {
    wtf_size_t start_offset;
    wtf_size_t end_offset;
  };

  std::optional<MarkerOffsets> ComputeOffsetsAfterShift(
      wtf_size_t offset,
      wtf_size_t old_length,
      wtf_size_t new_length) const;

  // Offset modifications are done by DocumentMarkerController.
  // Other classes should not call following setters.
  void SetStartOffset(wtf_size_t offset) { start_offset_ = offset; }
  void SetEndOffset(wtf_size_t offset) { end_offset_ = offset; }
  void ShiftOffsets(int delta);

  virtual void Trace(Visitor* visitor) const {}

 protected:
  DocumentMarker(wtf_size_t start_offset, wtf_size_t end_offset);

 private:
  wtf_size_t start_offset_;
  wtf_size_t end_offset_;
};

using DocumentMarkerVector = HeapVector<Member<DocumentMarker>>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_DOCUMENT_MARKER_H_
