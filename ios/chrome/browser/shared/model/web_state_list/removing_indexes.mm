// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"

#import <algorithm>

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

int RemovingIndexes::EmptyStorage::Count() const {
  return 0;
}

bool RemovingIndexes::EmptyStorage::ContainsIndex(int index) const {
  return false;
}

int RemovingIndexes::EmptyStorage::IndexAfterRemoval(int index) const {
  return index;
}

RemovingIndexes::OneIndexStorage::OneIndexStorage(int index) : index_(index) {}

int RemovingIndexes::OneIndexStorage::Count() const {
  return 1;
}

bool RemovingIndexes::OneIndexStorage::ContainsIndex(int index) const {
  return index == index_;
}

int RemovingIndexes::OneIndexStorage::IndexAfterRemoval(int index) const {
  if (index == index_) {
    return WebStateList::kInvalidIndex;
  }

  if (index > index_) {
    return index - 1;
  }

  return index;
}

RemovingIndexes::RangeStorage::RangeStorage(int start, int count)
    : start_(start), count_(count) {}

int RemovingIndexes::RangeStorage::Count() const {
  return count_;
}

bool RemovingIndexes::RangeStorage::ContainsIndex(int index) const {
  return start_ <= index && index < start_ + count_;
}

int RemovingIndexes::RangeStorage::IndexAfterRemoval(int index) const {
  if (index < start_) {
    return index;
  }

  if (index - start_ < count_) {
    return WebStateList::kInvalidIndex;
  }

  return index - count_;
}

RemovingIndexes::VectorStorage::VectorStorage(std::vector<int> indexes)
    : indexes_(std::move(indexes)) {}

RemovingIndexes::VectorStorage::~VectorStorage() = default;

RemovingIndexes::VectorStorage::VectorStorage(const VectorStorage&) = default;

RemovingIndexes::VectorStorage& RemovingIndexes::VectorStorage::operator=(
    const VectorStorage&) = default;

RemovingIndexes::VectorStorage::VectorStorage(VectorStorage&&) = default;

RemovingIndexes::VectorStorage& RemovingIndexes::VectorStorage::operator=(
    VectorStorage&&) = default;

int RemovingIndexes::VectorStorage::Count() const {
  return indexes_.size();
}

bool RemovingIndexes::VectorStorage::ContainsIndex(int index) const {
  return std::binary_search(indexes_.begin(), indexes_.end(), index);
}

int RemovingIndexes::VectorStorage::IndexAfterRemoval(int index) const {
  const auto lower_bound =
      std::lower_bound(indexes_.begin(), indexes_.end(), index);

  if (lower_bound == indexes_.end() || *lower_bound != index) {
    return index - std::distance(indexes_.begin(), lower_bound);
  }

  return WebStateList::kInvalidIndex;
}

RemovingIndexes::RemovingIndexes(std::vector<int> indexes)
    : removing_(StorageFromVector(std::move(indexes))) {}

RemovingIndexes::RemovingIndexes(std::initializer_list<int> indexes)
    : removing_(StorageFromInitializerList(std::move(indexes))) {}

RemovingIndexes::RemovingIndexes(const RemovingIndexes&) = default;

RemovingIndexes& RemovingIndexes::operator=(const RemovingIndexes&) = default;

RemovingIndexes::RemovingIndexes(RemovingIndexes&&) = default;

RemovingIndexes& RemovingIndexes::operator=(RemovingIndexes&&) = default;

RemovingIndexes::~RemovingIndexes() = default;

// static
RemovingIndexes RemovingIndexes::Range(int start, int count) {
  if (count <= 0) {
    return RemovingIndexes(EmptyStorage());
  }

  if (count == 1) {
    return RemovingIndexes(OneIndexStorage(start));
  }

  return RemovingIndexes(RangeStorage(start, count));
}

int RemovingIndexes::count() const {
  return absl::visit([](const auto& storage) { return storage.Count(); },
                     removing_);
}

bool RemovingIndexes::Contains(int index) const {
  return absl::visit(
      [index](const auto& storage) { return storage.ContainsIndex(index); },
      removing_);
}

int RemovingIndexes::IndexAfterRemoval(int index) const {
  return absl::visit(
      [index](const auto& storage) { return storage.IndexAfterRemoval(index); },
      removing_);
}

RemovingIndexes::Storage RemovingIndexes::StorageFromVector(
    std::vector<int> indexes) {
  std::sort(indexes.begin(), indexes.end());
  indexes.erase(std::unique(indexes.begin(), indexes.end()), indexes.end());

  if (indexes.empty()) {
    return EmptyStorage();
  }

  if (indexes.size() == 1) {
    return OneIndexStorage(indexes[0]);
  }

  int start = indexes[0];
  int count = static_cast<int>(indexes.size());
  if (indexes.back() + 1 == start + count) {
    return RangeStorage(start, count);
  }

  return VectorStorage(indexes);
}

RemovingIndexes::Storage RemovingIndexes::StorageFromInitializerList(
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

RemovingIndexes::RemovingIndexes(Storage storage)
    : removing_(std::move(storage)) {}
