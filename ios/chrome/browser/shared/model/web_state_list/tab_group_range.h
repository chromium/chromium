// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_GROUP_RANGE_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_GROUP_RANGE_H_

#import <set>

#import "base/check_op.h"

// Represents the range of a TabGroup in its owning WebStateList.
class TabGroupRange {
 public:
  // Initializes the range with a start and count.
  constexpr TabGroupRange(int start, int count) : start_(start), count_(count) {
    DCHECK_GE(count_, 0);
  }

  // Returns a range that is invalid, which is {-1, 0}.
  static constexpr TabGroupRange InvalidRange() { return TabGroupRange(-1, 0); }

  // Checks if the range is valid, i.e. is not empty.
  constexpr bool valid() const { return count_ > 0; }

  // Getters.
  constexpr int range_begin() const { return start_; }
  constexpr int count() const { return count_; }
  std::set<int> AsSet() const { return std::set<int>(begin(), end()); }

  // `range_end` is the first index not in the range.
  constexpr int range_end() const { return start_ + count_; }
  // Whether the index is inside the range.
  constexpr bool contains(int index) const {
    return start_ <= index && index < start_ + count_;
  }

  // Updates the range by moving it. The count stays the same, but
  // `range_begin` increases by `delta`. If `delta` is positive, the group
  // moves to the right. Otherwise, it moves to the left.
  constexpr void Move(int delta) {
    if (delta < 0) {
      MoveLeft(-delta);
    } else {
      MoveRight(delta);
    }
  }

  // Updates the range by moving it in a given direction. By default, it moves
  // by one.
  constexpr void MoveLeft(int delta = 1) {
    CHECK_GE(delta, 0);
    CHECK_GE(start_, delta);
    start_ -= delta;
  }
  constexpr void MoveRight(int delta = 1) {
    CHECK_GE(delta, 0);
    CHECK_LT(start_, INT_MAX - delta);
    start_ += delta;
  }

  // Updates the range by expanding/contracting by one in a given direction.
  constexpr void ExpandLeft() {
    MoveLeft();
    ExpandRight();
  }
  constexpr void ExpandRight() { ++count_; }
  constexpr void ContractLeft() {
    MoveRight();
    ContractRight();
  }
  constexpr void ContractRight() {
    CHECK_GT(count_, 0);
    --count_;
  }

  constexpr bool operator==(const TabGroupRange& other) const = default;
  constexpr bool operator!=(const TabGroupRange& other) const = default;

  // Support for range-based for-loops. Ex:
  //
  //  TabGroupRange range = ...;
  //  for (int i : range) {
  //    // ... do something with the index from the range.
  //  }
  //
  class iterator {
   public:
    constexpr iterator(int current) : current_(current) {}

    int operator*() const { return current_; }
    void operator++() { ++current_; }
    bool operator!=(const iterator& other) const {
      return current_ != other.current_;
    }

   private:
    int current_;
  };
  iterator begin() const { return iterator{start_}; }
  iterator end() const { return iterator{range_end()}; }

 private:
  int start_;
  int count_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_GROUP_RANGE_H_
