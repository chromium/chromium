// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_ORDER_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_ORDER_CONTROLLER_H_

#include "base/memory/raw_ref.h"

class OrderControllerSource;
class RemovingIndexes;

// OrderController abstracts the algorithms used to determine the insertion
// index of a new element, or the selection of the new active index of a
// WebStateList, independently of its representation, thus allowing using
// it on a serialized version.
class OrderController {
 public:
  // Represents a range of indices.
  struct Range {
    const int begin;
    const int end;  // excluded
  };

  // Structure used to represent the requirement to determine the insertion
  // index of a new element in the list. The structure must be initialized
  // using one of the public factory methods.
  struct InsertionParams {
    const int desired_index;
    const int opener_index;
    const Range range;

    // Factory representing automatic selection of the insertion index.
    static InsertionParams Automatic(Range range);

    // Factory representing insertion at a specified index.
    static InsertionParams ForceIndex(int desired_index, Range range);

    // Factory representing insertion relative to the opener.
    static InsertionParams WithOpener(int opener_index, Range range);
  };

  explicit OrderController(const OrderControllerSource& source);
  ~OrderController();

  // Determines where to place a newly opened WebState according to the
  // requirements expressed by `params`.
  int DetermineInsertionIndex(InsertionParams params) const;

  // Determines where to shift the active index after a WebState is closed.
  //
  // The index returned does not take into consideration the elements to be
  // closed. If the calling code needs the index after closing the elements,
  // it should use RemovingIndexes::IndexAfterRemoval(...) on the returned
  // value.
  int DetermineNewActiveIndex(int active_index,
                              const RemovingIndexes& removing_indexes) const;

 private:
  raw_ref<const OrderControllerSource> source_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_ORDER_CONTROLLER_H_
