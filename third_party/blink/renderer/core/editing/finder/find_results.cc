// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/finder/find_results.h"

namespace blink {

FindResults::FindResults() {
  empty_result_ = true;
}

FindResults::FindResults(const FindBuffer& find_buffer,
                         TextSearcherICU* text_searcher,
                         const Vector<UChar>& buffer,
                         const String& search_text,
                         const FindOptions options) {
  // We need to own the |search_text| because |text_searcher_| only has a
  // StringView (doesn't own the search text).
  search_text_ = search_text;
  find_buffer_ = &find_buffer;
  text_searcher_ = text_searcher;
  text_searcher_->SetPattern(search_text_, options);
  text_searcher_->SetText(buffer.data(), buffer.size());
  text_searcher_->SetOffset(0);
}

FindResults::Iterator FindResults::begin() const {
  if (empty_result_) {
    return end();
  }
  text_searcher_->SetOffset(0);
  return Iterator(*find_buffer_, text_searcher_);
}

FindResults::Iterator FindResults::end() const {
  return Iterator();
}

bool FindResults::IsEmpty() const {
  return begin() == end();
}

FindResults::BufferMatchResult FindResults::front() const {
  return *begin();
}

FindResults::BufferMatchResult FindResults::back() const {
  Iterator last_result;
  for (Iterator it = begin(); it != end(); ++it) {
    last_result = it;
  }
  return *last_result;
}

unsigned FindResults::CountForTesting() const {
  unsigned result = 0;
  for (Iterator it = begin(); it != end(); ++it) {
    ++result;
  }
  return result;
}

// FindResults::Iterator implementation.
FindResults::Iterator::Iterator(const FindBuffer& find_buffer,
                                TextSearcherICU* text_searcher)
    : find_buffer_(&find_buffer),
      text_searcher_(text_searcher),
      match_({0u, 0u}) {
  operator++();
}

const FindResults::BufferMatchResult FindResults::Iterator::operator*() const {
  DCHECK(match_);
  return FindResults::BufferMatchResult({match_->start, match_->length});
}

void FindResults::Iterator::operator++() {
  DCHECK(match_);
  match_ = text_searcher_->NextMatchResult();
  if (match_ && find_buffer_ && find_buffer_->IsInvalidMatch(*match_)) {
    operator++();
  }
}

}  // namespace blink
