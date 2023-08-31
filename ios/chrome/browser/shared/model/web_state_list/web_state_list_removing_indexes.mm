// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_removing_indexes.h"

#import <algorithm>

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

int WebStateListRemovingIndexes::EmptyStorage::Count() const {
  return 0;
}

bool WebStateListRemovingIndexes::EmptyStorage::ContainsIndex(int index) const {
  return false;
}

int WebStateListRemovingIndexes::EmptyStorage::IndexAfterRemoval(
    int index) const {
  return index;
}

WebStateListRemovingIndexes::OneIndexStorage::OneIndexStorage(int index)
    : index_(index) {}

int WebStateListRemovingIndexes::OneIndexStorage::Count() const {
  return 1;
}

bool WebStateListRemovingIndexes::OneIndexStorage::ContainsIndex(
    int index) const {
  return index == index_;
}

int WebStateListRemovingIndexes::OneIndexStorage::IndexAfterRemoval(
    int index) const {
  if (index == index_) {
    return WebStateList::kInvalidIndex;
  }

  if (index > index_) {
    return index - 1;
  }

  return index;
}

WebStateListRemovingIndexes::VectorStorage::VectorStorage(
    std::vector<int> indexes)
    : indexes_(std::move(indexes)) {}

WebStateListRemovingIndexes::VectorStorage::~VectorStorage() = default;

WebStateListRemovingIndexes::VectorStorage::VectorStorage(
    const VectorStorage&) = default;

WebStateListRemovingIndexes::VectorStorage&
WebStateListRemovingIndexes::VectorStorage::operator=(const VectorStorage&) =
    default;

WebStateListRemovingIndexes::VectorStorage::VectorStorage(VectorStorage&&) =
    default;

WebStateListRemovingIndexes::VectorStorage&
WebStateListRemovingIndexes::VectorStorage::operator=(VectorStorage&&) =
    default;

int WebStateListRemovingIndexes::VectorStorage::Count() const {
  return indexes_.size();
}

bool WebStateListRemovingIndexes::VectorStorage::ContainsIndex(
    int index) const {
  return std::binary_search(indexes_.begin(), indexes_.end(), index);
}

int WebStateListRemovingIndexes::VectorStorage::IndexAfterRemoval(
    int index) const {
  const auto lower_bound =
      std::lower_bound(indexes_.begin(), indexes_.end(), index);

  if (lower_bound == indexes_.end() || *lower_bound != index) {
    return index - std::distance(indexes_.begin(), lower_bound);
  }

  return WebStateList::kInvalidIndex;
}

WebStateListRemovingIndexes::WebStateListRemovingIndexes(
    std::vector<int> indexes)
    : removing_(StorageFromVector(std::move(indexes))) {}

WebStateListRemovingIndexes::WebStateListRemovingIndexes(
    std::initializer_list<int> indexes)
    : removing_(StorageFromInitializerList(std::move(indexes))) {}

WebStateListRemovingIndexes::WebStateListRemovingIndexes(
    const WebStateListRemovingIndexes&) = default;

WebStateListRemovingIndexes& WebStateListRemovingIndexes::operator=(
    const WebStateListRemovingIndexes&) = default;

WebStateListRemovingIndexes::WebStateListRemovingIndexes(
    WebStateListRemovingIndexes&&) = default;

WebStateListRemovingIndexes& WebStateListRemovingIndexes::operator=(
    WebStateListRemovingIndexes&&) = default;

WebStateListRemovingIndexes::~WebStateListRemovingIndexes() = default;

int WebStateListRemovingIndexes::count() const {
  return absl::visit([](const auto& storage) { return storage.Count(); },
                     removing_);
}

bool WebStateListRemovingIndexes::Contains(int index) const {
  return absl::visit(
      [index](const auto& storage) { return storage.ContainsIndex(index); },
      removing_);
}

int WebStateListRemovingIndexes::IndexAfterRemoval(int index) const {
  return absl::visit(
      [index](const auto& storage) { return storage.IndexAfterRemoval(index); },
      removing_);
}

WebStateListRemovingIndexes::Storage
WebStateListRemovingIndexes::StorageFromVector(std::vector<int> indexes) {
  std::sort(indexes.begin(), indexes.end());
  indexes.erase(std::unique(indexes.begin(), indexes.end()), indexes.end());

  if (indexes.empty()) {
    return EmptyStorage();
  }

  if (indexes.size() == 1) {
    return OneIndexStorage(indexes[0]);
  }

  return VectorStorage(indexes);
}

WebStateListRemovingIndexes::Storage
WebStateListRemovingIndexes::StorageFromInitializerList(
    std::initializer_list<int> indexes) {
  if (indexes.size() == 0) {
    return EmptyStorage();
  }

  if (indexes.size() == 1) {
    return OneIndexStorage(*indexes.begin());
  }

  // Use the vector overload.
  return StorageFromVector(std::vector<int>(std::move(indexes)));
}
