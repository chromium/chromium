// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_INLINE_ITEMS_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_INLINE_ITEMS_DATA_H_

#include <optional>

#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_text_index.h"
#include "third_party/blink/renderer/platform/wtf/text/text_offset_map.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct InlineItemsDataWithOffsetMap;
class InlineItemSegments;
struct InlineNodeData;
class OffsetMapping;

// Represents a text content with a list of InlineItem. A node may have an
// additional InlineItemsData for ::first-line pseudo-element.
struct CORE_EXPORT InlineItemsData : public GarbageCollected<InlineItemsData> {
 public:
  InlineItemTextIndex End() const {
    return {items.size(), text_content.length()};
  }

  // Text content for all inline items represented by a single InlineNode.
  // Encoded either as UTF-16 or latin-1 depending on the content.
  String text_content;
  InlineItems items;

  // Cache RunSegmenter segments when at least one item has multiple runs.
  // Set to nullptr when all items has only single run, which is common case for
  // most writing systems. However, in multi-script writing systems such as
  // Japanese, almost every item has multiple runs.
  Member<InlineItemSegments> segments;

  // The DOM to text content offset mapping of this inline node.
  Member<OffsetMapping> offset_mapping;

  bool IsValidOffset(unsigned index, unsigned offset) const {
    return index < items.size() && items[index]->IsValidOffset(offset);
  }
  bool IsValidOffset(const InlineItemTextIndex& index) const {
    return IsValidOffset(index.item_index, index.text_offset);
  }

  void AssertOffset(unsigned index, unsigned offset) const {
    items[index]->AssertOffset(offset);
  }
  void AssertOffset(const InlineItemTextIndex& index) const {
    AssertOffset(index.item_index, index.text_offset);
  }
  void AssertEndOffset(unsigned index, unsigned offset) const {
    items[index]->AssertEndOffset(offset);
  }

  // Get a list of `kOpenTag` items between `start_index` to
  // `start_index + size`.
  using OpenTagItems = HeapVector<Member<InlineItem>, 16>;
  void GetOpenTagItems(wtf_size_t start_index,
                       wtf_size_t size,
                       OpenTagItems* open_items) const;

  // When the `::first-line` style changed the text length, this returns a
  // `TextOffsetMap` that translates text offsets. Otherwise `nullopt`.
  const std::optional<TextOffsetMap>& OffsetMap() const;

#if DCHECK_IS_ON()
  void CheckConsistency() const;
#endif

  void LogCapacity() {
#if EXPENSIVE_DCHECKS_ARE_ON()
    items_capacity_after_shrink_ = items.capacity();
#endif
  }
  void ValidateCapacity() const {
#if EXPENSIVE_DCHECKS_ARE_ON()
    DCHECK(!items_capacity_after_shrink_ ||
           items.capacity() <= *items_capacity_after_shrink_)
        << "items grew from " << *items_capacity_after_shrink_ << " to "
        << items.capacity()
        << " after InlineNode::PrepareLayout() shrank it to fit";
#endif
  }

  InlineItemsData() = default;

  void Trace(Visitor* visitor) const;
  void TraceAfterDispatch(Visitor* visitor) const;

 protected:
  enum class DataType : uint8_t {
    kBase,
    kNodeData,
    kWithOffsetMap,
  };

  friend struct DowncastTraits<InlineNodeData>;
  friend struct DowncastTraits<InlineItemsDataWithOffsetMap>;

  explicit InlineItemsData(DataType data_type) : data_type_(data_type) {}
  bool IsNodeData() const { return data_type_ == DataType::kNodeData; }
  bool IsWithOffsetMap() const {
    return data_type_ == DataType::kWithOffsetMap;
  }

 private:
  DataType data_type_ = DataType::kBase;

#if EXPENSIVE_DCHECKS_ARE_ON()
  std::optional<wtf_size_t> items_capacity_after_shrink_;
#endif
};

// Represents inline items data with a text offset map.
// Used for `::first-line` styles that change the text lengths.
struct CORE_EXPORT InlineItemsDataWithOffsetMap : public InlineItemsData {
 public:
  InlineItemsDataWithOffsetMap() : InlineItemsData(DataType::kWithOffsetMap) {}

  void TraceAfterDispatch(Visitor* visitor) const {
    InlineItemsData::TraceAfterDispatch(visitor);
  }

  std::optional<TextOffsetMap> offset_map;
};

template <>
struct DowncastTraits<InlineItemsDataWithOffsetMap> {
  static bool AllowFrom(const InlineItemsData& value) {
    return value.IsWithOffsetMap();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_INLINE_ITEMS_DATA_H_
