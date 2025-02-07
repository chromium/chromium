// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_INLINE_ITEM_PTR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_INLINE_ITEM_PTR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_items_data.h"

namespace blink {

//
// A pointer to an `InlineItem` in a vector.
//
// When `InlineItem`s are in a vector, pointers to them have a risk of not being
// updated when the vector buffer is moved or reclaimed because `InlineItem` is
// a traceable disallow_new object. See crbug.com/389707047.
//
class CORE_EXPORT InlineItemPtr {
  DISALLOW_NEW();

 public:
  InlineItemPtr() = default;
  InlineItemPtr(wtf_size_t index, const InlineItemsData& items_data)
      : items_data_(&items_data), index_(index) {
    DCHECK_LT(index, items_data.items.size());
  }
  InlineItemPtr(const InlineItem& item, const InlineItemsData& items_data)
      : InlineItemPtr(std::distance(items_data.items.data(), &item),
                      items_data) {}

  // Get the pointer.
  const InlineItem* Get() const {
    return items_data_ ? &items_data_->items[index_] : nullptr;
  }
  const InlineItem* operator->() const { return Get(); }
  const InlineItem& operator*() const { return *Get(); }
  explicit operator const InlineItem*() const { return Get(); }
  explicit operator bool() const { return items_data_; }

  void Trace(Visitor* visitor) const;

 private:
  Member<const InlineItemsData> items_data_;
  wtf_size_t index_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_INLINE_ITEM_PTR_H_
