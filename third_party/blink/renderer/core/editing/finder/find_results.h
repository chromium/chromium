// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FINDER_FIND_RESULTS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FINDER_FIND_RESULTS_H_

#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"
#include "third_party/blink/renderer/core/editing/iterators/text_searcher_icu.h"

namespace blink {

// All match results for this buffer. We can iterate through the
// BufferMatchResults one by one using the Iterator.
class CORE_EXPORT FindResults {
  STACK_ALLOCATED();

 public:
  // A match result, containing the starting position of the match and
  // the length of the match.
  struct BufferMatchResult {
    const unsigned start;
    const unsigned length;

    bool operator==(const BufferMatchResult& other) const {
      return start == other.start && length == other.length;
    }

    bool operator!=(const BufferMatchResult& other) const {
      return !operator==(other);
    }
  };

  class CORE_EXPORT Iterator {
    STACK_ALLOCATED();

   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = BufferMatchResult;
    using difference_type = std::ptrdiff_t;
    using pointer = BufferMatchResult*;
    using reference = BufferMatchResult&;

    Iterator() = default;
    Iterator(const FindBuffer& find_buffer, TextSearcherICU* text_searcher);

    bool operator==(const Iterator& other) const {
      return has_match_ == other.has_match_;
    }

    bool operator!=(const Iterator& other) const {
      return has_match_ != other.has_match_;
    }

    const BufferMatchResult operator*() const;

    void operator++();

   private:
    const FindBuffer* find_buffer_;
    TextSearcherICU* text_searcher_;
    MatchResultICU match_;
    bool has_match_ = false;
  };

  FindResults();
  FindResults(const FindBuffer& find_buffer,
              TextSearcherICU* text_searcher,
              const Vector<UChar>& buffer,
              const String& search_text,
              const FindOptions options);

  Iterator begin() const;
  Iterator end() const;

  bool IsEmpty() const;

  BufferMatchResult front() const;
  BufferMatchResult back() const;

  unsigned CountForTesting() const;

 private:
  String search_text_;
  const FindBuffer* find_buffer_;
  TextSearcherICU* text_searcher_;
  bool empty_result_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FINDER_FIND_RESULTS_H_
