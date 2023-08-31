// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_REMOVING_INDEXES_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_REMOVING_INDEXES_H_

#include <initializer_list>
#include <vector>

#include "third_party/abseil-cpp/absl/types/variant.h"

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

 private:
  // Represents an empty WebStateListRemovingIndexes.
  class EmptyStorage {
   public:
    // Returns the number of items to remove.
    int Count() const;

    // Returns whether `index` will be removed.
    bool ContainsIndex(int index) const;

    // Returns the updated value of `index` after items have been removed.
    int IndexAfterRemoval(int index) const;
  };

  // Represents a WebStateListRemovingIndexes with a single index.
  class OneIndexStorage {
   public:
    OneIndexStorage(int index);

    // Returns the number of items to remove.
    int Count() const;

    // Returns whether `index` will be removed.
    bool ContainsIndex(int index) const;

    // Returns the updated value of `index` after items have been removed.
    int IndexAfterRemoval(int index) const;

   private:
    int index_;
  };

  // Represents a WebStateListRemovingIndexes with two or more indexes.
  class VectorStorage {
   public:
    VectorStorage(std::vector<int> indexes);
    ~VectorStorage();

    VectorStorage(const VectorStorage&);
    VectorStorage& operator=(const VectorStorage&);

    VectorStorage(VectorStorage&&);
    VectorStorage& operator=(VectorStorage&&);

    // Returns the number of items to remove.
    int Count() const;

    // Returns whether `index` will be removed.
    bool ContainsIndex(int index) const;

    // Returns the updated value of `index` after items have been removed.
    int IndexAfterRemoval(int index) const;

   private:
    std::vector<int> indexes_;
  };

  // Alias for the variant storing the indexes to remove. Using a variant
  // allow not allocating for the common case of removing one element.
  using Storage = absl::variant<EmptyStorage, OneIndexStorage, VectorStorage>;

  // Helper methods to create the storage from a vector or and initializer list.
  static Storage StorageFromVector(std::vector<int> indexes);
  static Storage StorageFromInitializerList(std::initializer_list<int> indexes);

  Storage removing_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_REMOVING_INDEXES_H_
