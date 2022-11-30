// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEMS_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEMS_DATA_H_

#include <memory>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class NGInlineItemSegments;
class NGOffsetMapping;

// Represents a text content with a list of NGInlineItem. A node may have an
// additional NGInlineItemsData for ::first-line pseudo element.
struct CORE_EXPORT NGInlineItemsData
    : public GarbageCollected<NGInlineItemsData> {
 public:
  virtual ~NGInlineItemsData() = default;

  // Text content for all inline items represented by a single NGInlineNode.
  // Encoded either as UTF-16 or latin-1 depending on the content.
  String text_content;
  HeapVector<NGInlineItem> items;

  // Cache RunSegmenter segments when at least one item has multiple runs.
  // Set to nullptr when all items has only single run, which is common case for
  // most writing systems. However, in multi-script writing systems such as
  // Japanese, almost every item has multiple runs.
  std::unique_ptr<NGInlineItemSegments> segments;

  // The DOM to text content offset mapping of this inline node.
  Member<NGOffsetMapping> offset_mapping;

  bool IsValidOffset(unsigned index, unsigned offset) const {
    return index < items.size() && items[index].IsValidOffset(offset);
  }

  void AssertOffset(unsigned index, unsigned offset) const {
    items[index].AssertOffset(offset);
  }
  void AssertEndOffset(unsigned index, unsigned offset) const {
    items[index].AssertEndOffset(offset);
  }

  // Get a list of |kOpenTag| that are open at |size|.
  using OpenTagItems = Vector<const NGInlineItem*, 16>;
  void GetOpenTagItems(wtf_size_t size, OpenTagItems* open_items) const;

  virtual void Trace(Visitor* visitor) const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEMS_DATA_H_
