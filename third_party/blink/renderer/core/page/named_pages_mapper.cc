// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/named_pages_mapper.h"

namespace blink {

void NamedPagesMapper::AddNamedPage(const AtomicString& page_name,
                                    int page_index) {
  DCHECK_GE(entries_.size(), 1u);

  // The last entry should have no last page set.
  DCHECK_EQ(entries_.back().last_page_index, -1);

  if (page_index > 0) {
    auto prev_it = entries_.rbegin();
    for (auto it = prev_it + 1; it != entries_.rend(); prev_it = it++) {
      if (it->last_page_index < page_index)
        break;
    }

    entries_.Shrink(static_cast<wtf_size_t>(entries_.rend() - prev_it));

    // Terminate the previous entry (now that we know its last page index)
    // before adding the new entry.
    entries_.back().last_page_index = page_index - 1;
  } else {
    DCHECK_EQ(page_index, 0);
    entries_.Shrink(0);
  }
  entries_.emplace_back(page_name);
}

void NamedPagesMapper::NameFirstPage(const AtomicString& page_name) {
  DCHECK_GE(entries_.size(), 1u);
  entries_.front().page_name = page_name;
}

const AtomicString& NamedPagesMapper::NamedPageAtIndex(int page_index) const {
  for (const Entry& entry : entries_) {
    if (page_index <= entry.last_page_index)
      return entry.page_name;
  }
  return entries_.back().page_name;
}

}  // namespace blink
