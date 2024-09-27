// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/finder/find_results.h"

namespace blink {

FindResults::FindResults() {
  empty_result_ = true;
}

FindResults::FindResults(const FindBuffer* find_buffer,
                         TextSearcherICU* text_searcher,
                         const Vector<UChar>& buffer,
                         const Vector<Vector<UChar>>* extra_buffers,
                         const String& search_text,
                         const FindOptions options) {
  // We need to own the |search_text| because |text_searcher_| only has a
  // StringView (doesn't own the search text).
  search_text_ = search_text;
  find_buffer_ = find_buffer;
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
  return Iterator(find_buffer_, text_searcher_, extra_searchers_);
}

FindResults::Iterator FindResults::end() const {
  return Iterator();
}

bool FindResults::IsEmpty() const {
  return begin() == end();
}

MatchResultICU FindResults::front() const {
  return *begin();
}

MatchResultICU FindResults::back() const {
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
FindResults::Iterator::Iterator(
    const FindBuffer* find_buffer,
    TextSearcherICU* text_searcher,
    const Vector<std::unique_ptr<TextSearcherICU>>& extra_searchers)
    : find_buffer_(find_buffer) {
  text_searcher_list_.reserve(1 + extra_searchers.size());
  text_searcher_list_.push_back(text_searcher);
  // Initialize match_list_ with a value so that IsAtEnd() returns false.
  match_list_.push_back(std::optional<MatchResultICU>({0, 0}));
  for (const auto& searcher : extra_searchers) {
    text_searcher_list_.push_back(searcher.get());
    match_list_.push_back(std::optional<MatchResultICU>({0, 0}));
  }
  operator++();
}

std::optional<MatchResultICU> FindResults::Iterator::EarliestMatch() const {
  auto min_iter = std::min_element(match_list_.begin(), match_list_.end(),
                                   [](const auto& a, const auto& b) {
                                     if (a.has_value() && !b.has_value()) {
                                       return true;
                                     }
                                     if (!a.has_value() || !b.has_value()) {
                                       return false;
                                     }
                                     return a->start < b->start;
                                   });
  std::optional<MatchResultICU> result;
  if (min_iter != match_list_.end() && min_iter->has_value()) {
    result.emplace((**min_iter).start, (**min_iter).length);
  }
  return result;
}

const MatchResultICU FindResults::Iterator::operator*() const {
  DCHECK(!IsAtEnd());
  std::optional<MatchResultICU> result = EarliestMatch();
  return *result;
}

void FindResults::Iterator::operator++() {
  DCHECK(!IsAtEnd());
  const MatchResultICU last_result = **this;
  for (size_t i = 0; i < text_searcher_list_.size(); ++i) {
    auto& optional_match = match_list_[i];
    if (optional_match.has_value() &&
        optional_match->start == last_result.start) {
      optional_match = text_searcher_list_[i]->NextMatchResult();
    }
  }
  std::optional<MatchResultICU> match = EarliestMatch();
  if (match && find_buffer_ && find_buffer_->IsInvalidMatch(*match)) {
    operator++();
  }
}

bool FindResults::Iterator::IsAtEnd() const {
  // True if match_list_ contains no valid values.
  for (const auto& opt_match : match_list_) {
    if (opt_match.has_value()) {
      return false;
    }
  }
  return true;
}

}  // namespace blink
