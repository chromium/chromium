// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEM_SPAN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEM_SPAN_H_

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_items_data.h"

namespace blink {
// |NGInlineItemSpan| performs like base::span<NGInlineItem> but stores a
// pointer to |NGInlineItemsData|, not |HeapVector<NGInlineItem>| in
// |NGInlineItemsData|, to keep it alive. |HeapVector<NGInlineItem>| allocated
// in |NGInlineItemsData| is deleted in |NGInlineItemsData|'s destructor even
// if there is any pointer to the vector, and storing a pointer to it can
// cause use-after-free bugs.
struct NGInlineItemSpan final {
  DISALLOW_NEW();

 public:
  NGInlineItemSpan() = default;

  void SetItems(NGInlineItemsData* data, wtf_size_t begin, wtf_size_t size) {
    SECURITY_DCHECK(begin < data->items.size());
    SECURITY_DCHECK(begin_ + size_ <= data->items.size());
    data_ = data;
    begin_ = begin;
    size_ = size;
  }
  void Clear() {
    data_ = nullptr;
    begin_ = 0;
    size_ = 0;
  }

  bool empty() const { return size_ == 0; }
  wtf_size_t size() const { return size_; }

  const NGInlineItem* begin() const {
    SECURITY_DCHECK(begin_ < data_->items.size());
    return data_->items.begin() + begin_;
  }
  const NGInlineItem* end() const {
    SECURITY_DCHECK(begin_ + size_ <= data_->items.size());
    return begin() + size_;
  }

  const NGInlineItem& front() const {
    CHECK(!empty());
    const NGInlineItem* begin_ptr = begin();
    CHECK(begin_ptr);
    return *begin_ptr;
  }

  void Trace(Visitor* visitor) const { visitor->Trace(data_); }

 private:
  Member<NGInlineItemsData> data_;
  wtf_size_t begin_ = 0;
  wtf_size_t size_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEM_SPAN_H_
