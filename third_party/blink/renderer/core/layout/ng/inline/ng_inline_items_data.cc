// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_items_data.h"

namespace blink {

void NGInlineItemsData::GetOpenTagItems(wtf_size_t size,
                                        OpenTagItems* open_items) const {
  DCHECK_LE(size, items.size());
  for (const NGInlineItem& item : base::make_span(items.data(), size)) {
    if (item.Type() == NGInlineItem::kOpenTag)
      open_items->push_back(&item);
    else if (item.Type() == NGInlineItem::kCloseTag)
      open_items->pop_back();
  }
}

void NGInlineItemsData::Trace(Visitor* visitor) const {
  visitor->Trace(items);
  visitor->Trace(offset_mapping);
}

}  // namespace blink
