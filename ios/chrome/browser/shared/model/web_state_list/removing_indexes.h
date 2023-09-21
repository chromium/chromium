// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_REMOVING_INDEXES_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_REMOVING_INDEXES_H_

#include <initializer_list>
#include <vector>

#include "third_party/abseil-cpp/absl/types/variant.h"

// RemovingIndexes is a class storing a list of indexes that will soon be
// closed in a WebStateList (or a serialized representation) and providing
// support methods to fix the indexes of other WebStates.
//
// The class has optimization for lists of exactly zero and one indexes as
// they are common (e.g. closing one tab, or closing tabs matching some
// criteria).
class RemovingIndexes {
 public:
  explicit RemovingIndexes(std::vector<int> indexes);
  RemovingIndexes(std::initializer_list<int> indexes);

  RemovingIndexes(const RemovingIndexes&);
  RemovingIndexes& operator=(const RemovingIndexes&);

  RemovingIndexes(RemovingIndexes&&);
  RemovingIndexes& operator=(RemovingIndexes&&);

  ~RemovingIndexes();

  // Returns the number of WebState that will be closed.
  int count() const;

  // Returns whether index is present in the list of indexes to close.
  bool Contains(int index) const;

  // Returns the new value of index after the removal. For indexes that are
  // scheduled to be removed, will return WebStateList::kInvalidIndex.
  int IndexAfterRemoval(int index) const;

 private:
  // Represents an empty RemovingIndexes.
  class EmptyStorage {
   public:
    // Returns the number of items to remove.
    int Count() const;

    // Returns whether `index` will be removed.
    bool ContainsIndex(int index) const;

    // Returns the updated value of `index` after items have been removed.
    int IndexAfterRemoval(int index) const;
  };

  // Represents a RemovingIndexes with a single index.
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

  // Represents a RemovingIndexes with two or more indexes.
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

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_REMOVING_INDEXES_H_
