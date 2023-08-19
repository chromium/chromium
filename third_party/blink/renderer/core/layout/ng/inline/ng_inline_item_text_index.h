// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEM_TEXT_INDEX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEM_TEXT_INDEX_H_

#include <ostream>
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

// Represents an index of `NGInlineItem`, along with the text offset.
struct CORE_EXPORT NGInlineItemTextIndex {
  explicit operator bool() const { return text_offset || item_index; }
  bool IsZero() const { return !text_offset && !item_index; }

  bool operator==(const NGInlineItemTextIndex& other) const {
    return text_offset == other.text_offset && item_index == other.item_index;
  }
  bool operator!=(const NGInlineItemTextIndex& other) const {
    return !operator==(other);
  }
  bool operator>(const NGInlineItemTextIndex& other) const {
    return text_offset > other.text_offset || item_index > other.item_index;
  }
  bool operator<(const NGInlineItemTextIndex& other) const {
    return text_offset < other.text_offset || item_index < other.item_index;
  }
  bool operator>=(const NGInlineItemTextIndex& other) const {
    return !operator<(other);
  }
  bool operator<=(const NGInlineItemTextIndex& other) const {
    return !operator>(other);
  }

  // The index of `NGInlineItemsData::items`.
  wtf_size_t item_index = 0;
  // The offset of `NGInlineItemsData::text_content`.
  wtf_size_t text_offset = 0;
};

inline std::ostream& operator<<(std::ostream& ostream,
                                const NGInlineItemTextIndex& index) {
  return ostream << "{" << index.item_index << "," << index.text_offset << "}";
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEM_TEXT_INDEX_H_
