// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_CONSECUTIVE_RANGE_VISITOR_H_
#define COURGETTE_CONSECUTIVE_RANGE_VISITOR_H_

#include <stddef.h>

#include <iterator>

namespace courgette {

// Usage note: First check whether std::unique() would suffice.
//
// ConsecutiveRangeVisitor is a visitor to read equal consecutive items
// ("ranges") between two iterators. The base value of InputIterator must
// implement the == operator.
//
// Example: "AAAAABZZZZOO" consists of ranges ["AAAAA", "B", "ZZZZ", "OO"]. The
// visitor provides accessors to iterate through the ranges, and to access each
// range's value and repeat, i.e., [('A', 5), ('B', 1), ('Z', 4), ('O', 2)].
template <class InputIterator>
class ConsecutiveRangeVisitor {
 public:
  ConsecutiveRangeVisitor(InputIterator begin, InputIterator end)
      : head_(begin), end_(end) {
    advance();
  }

  ConsecutiveRangeVisitor(const ConsecutiveRangeVisitor&) = delete;
  ConsecutiveRangeVisitor& operator=(const ConsecutiveRangeVisitor&) = delete;

  // Returns whether there are more ranges to traverse.
  bool has_more() const { return tail_ != end_; }

  // Returns an iterator to an element in the current range.
  InputIterator cur() const { return tail_; }

  // Returns the number of repeated elements in the current range.
  size_t repeat() const { return std::distance(tail_, head_); }

  // Advances to the next range.
  void advance() {
    tail_ = head_;
    if (head_ != end_)
      while (++head_ != end_ && *head_ == *tail_) {}
  }

 private:
  InputIterator tail_;  // The trailing pionter of a range (inclusive).
  InputIterator head_;  // The leading pointer of a range (exclusive).
  InputIterator end_;   // Store the end pointer so we know when to stop.
};

}  // namespace courgette

#endif  // COURGETTE_CONSECUTIVE_RANGE_VISITOR_H_
