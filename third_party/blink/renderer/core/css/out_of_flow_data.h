// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_OUT_OF_FLOW_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_OUT_OF_FLOW_DATA_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/successful_position_fallback.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/core/style/position_try_fallbacks.h"
#include "third_party/blink/renderer/platform/geometry/physical_offset.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class CSSPropertyValueSet;
class Element;
class LayoutBox;
class LayoutObject;

class CORE_EXPORT OutOfFlowData final
    : public GarbageCollected<OutOfFlowData>,
      public ElementRareDataField {
 public:
  struct ScrollOffsetPair {
    PhysicalOffset scroll_offset_for_layout;
    PhysicalOffset scroll_offset_for_range_adjustment;

    bool operator==(const ScrollOffsetPair& other) const {
      return scroll_offset_for_layout == other.scroll_offset_for_layout &&
             scroll_offset_for_range_adjustment ==
                 other.scroll_offset_for_range_adjustment;
    }
  };

  class RememberedScrollOffsets
      : public GarbageCollected<RememberedScrollOffsets> {
   public:
    RememberedScrollOffsets() = default;

    std::optional<ScrollOffsetPair> GetOffsetsForAnchor(
        const Element* anchor) const {
      if (!anchor) {
        return std::nullopt;
      }
      auto it = offsets_.find(anchor);
      return it != offsets_.end() ? std::make_optional(it->value)
                                  : std::nullopt;
    }
    std::optional<PhysicalOffset> GetOffsetForAnchor(
        const Element* anchor) const {
      if (const auto& offsets = GetOffsetsForAnchor(anchor)) {
        return offsets->scroll_offset_for_layout;
      }
      return std::nullopt;
    }
    std::optional<PhysicalOffset> GetOffsetForAnchorForRangeAdjustment(
        const Element* anchor) const {
      if (const auto& offsets = GetOffsetsForAnchor(anchor)) {
        return offsets->scroll_offset_for_range_adjustment;
      }
      return std::nullopt;
    }
    void SetOffsetsForAnchor(const Element* anchor,
                             const ScrollOffsetPair& offsets) {
      offsets_.Set(anchor, offsets);
    }

    bool operator==(const RememberedScrollOffsets& other) const {
      return offsets_ == other.offsets_;
    }

    void Trace(Visitor* visitor) const { visitor->Trace(offsets_); }

    String ToString() const;

   private:
    HeapHashMap<WeakMember<const Element>, ScrollOffsetPair> offsets_;
  };

  // For each layout of an OOF that ever had a successful try fallback, register
  // the current fallback. When ApplyPendingSuccessfulPositionFallback() is
  // called, update the last successful one.
  bool SetPendingSuccessfulPositionFallback(
      const PositionTryFallbacks* fallbacks,
      const CSSPropertyValueSet* try_set,
      const TryTacticList& try_tactics,
      std::optional<size_t> index);

  bool ClearPendingSuccessfulPositionFallback() {
    return SetPendingSuccessfulPositionFallback(nullptr, nullptr, kNoTryTactics,
                                                std::nullopt);
  }

  // At resize observer timing, update the last successful try fallback.
  // Returns true if last successful fallback was cleared.
  bool ApplyPendingSuccessfulPositionFallbackAndAnchorScrollShift(
      LayoutObject* layout_object);

  bool HasLastSuccessfulPositionFallback() const {
    return last_successful_position_fallback_.position_try_fallbacks_ !=
           nullptr;
  }

  // Clears the last successful position fallback if position-try-fallbacks
  // refer to any of the @position-try names passed in. Returns true if the last
  // successful fallbacks was cleared.
  bool InvalidatePositionTryNames(const HashSet<AtomicString>& try_names);

  const CSSPropertyValueSet* GetLastSuccessfulTrySet() const {
    return last_successful_position_fallback_.try_set_;
  }

  const TryTacticList& GetLastSuccessfulTryTactics() const {
    return last_successful_position_fallback_.try_tactics_;
  }

  std::optional<size_t> GetLastSuccessfulIndex() const {
    return last_successful_position_fallback_.index_;
  }

  std::optional<size_t> GetNewSuccessfulPositionFallbackIndex() const {
    if (new_successful_position_fallback_.index_ != std::nullopt) {
      return new_successful_position_fallback_.index_;
    }
    return last_successful_position_fallback_.index_;
  }

  // Return true if there's any stale successful position fallback data (if
  // `position-try-fallbacks` has changed).
  bool HasStaleFallbackData(const LayoutBox&) const;

  const RememberedScrollOffsets* GetRememberedScrollOffsets() const;
  const RememberedScrollOffsets* GetSpeculativeRememberedScrollOffsets() const;
  bool SetPendingRememberedScrollOffsets(const RememberedScrollOffsets*);

  void ClearRememberedScrollOffsets() {
    remembered_scroll_offsets_ = nullptr;
    pending_remembered_scroll_offsets_ = nullptr;
  }

  void Trace(Visitor*) const override;

 private:
  void ResetAnchorData();

  SuccessfulPositionFallback last_successful_position_fallback_;
  // If the previous layout had a successful position fallback, it is stored
  // here. Will be copied to the last_successful_position_fallback_ at next
  // resize observer update.
  SuccessfulPositionFallback new_successful_position_fallback_;

  Member<const RememberedScrollOffsets> remembered_scroll_offsets_;
  Member<const RememberedScrollOffsets> pending_remembered_scroll_offsets_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_OUT_OF_FLOW_DATA_H_
