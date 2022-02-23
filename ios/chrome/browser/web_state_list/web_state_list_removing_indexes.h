// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_REMOVING_INDEXES_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_REMOVING_INDEXES_H_

#include <initializer_list>
#include <vector>

#include "third_party/abseil-cpp/absl/types/variant.h"

class WebStateList;

namespace web {
class WebState;
}

// WebStateListRemovingIndexes is a class storing a list of indexes that will
// soon be closed in a WebStateList, and providing support method to fix the
// indexes of other WebStates. It is used by WebStateListOrderController to
// implement DetermineNewActiveIndex().
class WebStateListRemovingIndexes {
 public:
  explicit WebStateListRemovingIndexes(std::vector<int> indexes);
  WebStateListRemovingIndexes(std::initializer_list<int> indexes);

  WebStateListRemovingIndexes(const WebStateListRemovingIndexes&);
  WebStateListRemovingIndexes& operator=(const WebStateListRemovingIndexes&);

  WebStateListRemovingIndexes(WebStateListRemovingIndexes&&);
  WebStateListRemovingIndexes& operator=(WebStateListRemovingIndexes&&);

  ~WebStateListRemovingIndexes();

  // Returns the number of WebState that will be closed.
  int count() const;

  // Returns whether index is present in the list of indexes to close.
  bool Contains(int index) const;

  // Returns the new value of index after the removal. For indexes that are
  // scheduled to be removed, will return WebStateList::kInvalidIndex.
  int IndexAfterRemoval(int index) const;

  // Represents an empty WebStateListRemovingIndexes.
  struct Empty {};

  // Alias for the variant storing the indexes to remove. Using a variant
  // allow not allocating for the common case of removing one element.
  using Storage = absl::variant<Empty, int, std::vector<int>>;

 private:
  Storage removing_;
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_REMOVING_INDEXES_H_
