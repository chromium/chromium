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
                         const Vector<Vector<UChar>>* extra_buffers,
                         const String& search_text,
                         const FindOptions options) {
  // We need to own the |search_text| because |text_searcher_| only has a
  // StringView (doesn't own the search text).
  search_text_ = search_text;
  find_buffer_ = &find_buffer;
  text_searcher_ = text_searcher;
  text_searcher_->SetPattern(search_text_, options);
  text_searcher_->SetText(base::span(buffer));
  text_searcher_->SetOffset(0);
  if (!RuntimeEnabledFeatures::FindRubyInPageEnabled()) {
    DCHECK(!extra_buffers || extra_buffers->empty());
  } else if (extra_buffers) {
    extra_searchers_.reserve(extra_buffers->size());
    for (const auto& text : *extra_buffers) {
      extra_searchers_.push_back(
          std::make_unique<TextSearcherICU>(TextSearcherICU::kConstructLocal));
      auto& searcher = extra_searchers_.back();
      searcher->SetPattern(search_text_, options);
      searcher->SetText(base::span(text));
    }
  }
}

FindResults::Iterator FindResults::begin() const {
  if (empty_result_) {
    return end();
  }
  text_searcher_->SetOffset(0);
  for (auto& searcher : extra_searchers_) {
    searcher->SetOffset(0);
  }
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
      // Initialize match_ with a value so that IsAtEnd() returns false.
      match_({0u, 0u}) {
  operator++();
}

const FindResults::BufferMatchResult FindResults::Iterator::operator*() const {
  DCHECK(!IsAtEnd());
  return FindResults::BufferMatchResult({match_->start, match_->length});
}

void FindResults::Iterator::operator++() {
  DCHECK(!IsAtEnd());
  match_ = text_searcher_->NextMatchResult();
  if (match_ && find_buffer_ && find_buffer_->IsInvalidMatch(*match_)) {
    operator++();
  }
}

bool FindResults::Iterator::IsAtEnd() const {
  return !match_.has_value();
}

}  // namespace blink
