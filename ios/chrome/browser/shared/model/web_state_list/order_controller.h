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
  // Enumeration representing the group of the item during insertion.
  enum class ItemGroup {
    kRegular,
    kPinned,
  };

  // Structure used to represent the requirement to determine the insertion
  // index of a new element in the list. The structure must be initialized
  // using one of the public factory methods.
  struct InsertionParams {
    const int desired_index;
    const int opener_index;
    const ItemGroup group;

    // Returns whether the item is pinned.
    bool pinned() const { return group == ItemGroup::kPinned; }

    // Factory representing automatic selection of the insertion index.
    static InsertionParams Automatic(ItemGroup group);

    // Factory representing insertion at a specified index.
    static InsertionParams ForceIndex(int desired_index, ItemGroup group);

    // Factory representing insertion relative to the opener.
    static InsertionParams WithOpener(int opener_index, ItemGroup group);
  };

  explicit OrderController(const OrderControllerSource& source);
  ~OrderController();

  // Determines where to place a newly opened WebState according to the
  // requirements expressed by `params`.
  // Logic diagram: crbug.com/1395319
  int DetermineInsertionIndex(InsertionParams params) const;

  // Determines where to shift the active index after a WebState is closed.
  // The returned index will either be WebStateList::kInvalidIndex or in be
  // in range for the WebStateList once the element has been removed (i.e.
  // this function accounts for the fact that the element at `removing_index`
  // will be removed from the WebStateList).
  // Logic diagram: crbug.com/1395319
  int DetermineNewActiveIndex(int active_index,
                              const RemovingIndexes& removing_indexes) const;

 private:
  raw_ref<const OrderControllerSource> source_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_ORDER_CONTROLLER_H_
