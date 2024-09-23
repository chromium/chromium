// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"

#import <algorithm>

#import "ios/chrome/browser/shared/model/web_state_list/tab_group_range.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

int RemovingIndexes::EmptyStorage::Count() const {
  return 0;
}

RemovingIndexes::Range RemovingIndexes::EmptyStorage::Span() const {
  return {.start = -1, .count = 0};
}

bool RemovingIndexes::EmptyStorage::ContainsIndex(int index) const {
  return false;
}

int RemovingIndexes::EmptyStorage::IndexAfterRemoval(int index) const {
  return index;
}

TabGroupRange RemovingIndexes::EmptyStorage::RangeAfterRemoval(
    TabGroupRange range) const {
  CHECK(range.valid());
  return range;
}

RemovingIndexes::OneIndexStorage::OneIndexStorage(int index) : index_(index) {}

int RemovingIndexes::OneIndexStorage::Count() const {
  return 1;
}

RemovingIndexes::Range RemovingIndexes::OneIndexStorage::Span() const {
  return {.start = index_, .count = 1};
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

TabGroupRange RemovingIndexes::OneIndexStorage::RangeAfterRemoval(
    TabGroupRange range) const {
  CHECK(range.valid());

  if (index_ < range.range_begin()) {
    // The removed index is before the group: move the range to the left.
    return {range.range_begin() - 1, range.count()};
  }

  if (index_ < range.range_end()) {
    // The removed index is part of the group: contract the range.
    return {range.range_begin(), range.count() - 1};
  }

  return range;
}

RemovingIndexes::RangeStorage::RangeStorage(Range range) : range_(range) {}

int RemovingIndexes::RangeStorage::Count() const {
  return range_.count;
}

RemovingIndexes::Range RemovingIndexes::RangeStorage::Span() const {
  return range_;
}

bool RemovingIndexes::RangeStorage::ContainsIndex(int index) const {
  return range_.start <= index && index < range_.start + range_.count;
}

int RemovingIndexes::RangeStorage::IndexAfterRemoval(int index) const {
  if (index < range_.start) {
    return index;
  }

  if (index - range_.start < range_.count) {
    return WebStateList::kInvalidIndex;
  }

  return index - range_.count;
}

TabGroupRange RemovingIndexes::RangeStorage::RangeAfterRemoval(
    TabGroupRange range) const {
  CHECK(range.valid());

  // Compute the new starting point.
  const int new_begin = std::min(range.range_begin(), range_.start);
  // Compute the new count.
  const int intersection_begin = std::max(range.range_begin(), range_.start);
  const int intersection_end =
      std::min(range.range_end(), range_.start + range_.count);
  const int intersection_count = intersection_end - intersection_begin;
  const int new_count = range.count() - intersection_count;

  return {new_begin, new_count};
}

RemovingIndexes::VectorStorage::VectorStorage(std::vector<int> indexes)
    : indexes_(std::move(indexes)) {
  DCHECK_GE(indexes_.size(), 2u);
}

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

RemovingIndexes::Range RemovingIndexes::VectorStorage::Span() const {
  const int start = indexes_.front();
  const int count = indexes_.back() + 1 - start;
  return {.start = start, .count = count};
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

TabGroupRange RemovingIndexes::VectorStorage::RangeAfterRemoval(
    TabGroupRange range) const {
  CHECK(range.valid());

  const auto range_begin_lower_bound =
      std::lower_bound(indexes_.begin(), indexes_.end(), range.range_begin());
  const auto range_end_lower_bound = std::lower_bound(
      range_begin_lower_bound, indexes_.end(), range.range_end());

  // Compute the new starting point.
  const int new_begin =
      range.range_begin() -
      std::distance(indexes_.begin(), range_begin_lower_bound);

  // Compute the new count.
  const int intersection_count =
      std::distance(range_begin_lower_bound, range_end_lower_bound);
  const int new_count = range.count() - intersection_count;

  return {new_begin, new_count};
}

RemovingIndexes::RemovingIndexes(Range range)
    : removing_(StorageFromRange(range)) {}

RemovingIndexes::RemovingIndexes(std::vector<int> indexes)
    : removing_(StorageFromVector(std::move(indexes))) {}

RemovingIndexes::RemovingIndexes(std::initializer_list<int> indexes)
    : removing_(StorageFromInitializerList(std::move(indexes))) {}

RemovingIndexes::RemovingIndexes(const RemovingIndexes&) = default;

RemovingIndexes& RemovingIndexes::operator=(const RemovingIndexes&) = default;

RemovingIndexes::RemovingIndexes(RemovingIndexes&&) = default;

RemovingIndexes& RemovingIndexes::operator=(RemovingIndexes&&) = default;

RemovingIndexes::~RemovingIndexes() = default;

int RemovingIndexes::count() const {
  return absl::visit([](const auto& storage) { return storage.Count(); },
                     removing_);
}

RemovingIndexes::Range RemovingIndexes::span() const {
  return absl::visit([](const auto& storage) { return storage.Span(); },
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

TabGroupRange RemovingIndexes::RangeAfterRemoval(TabGroupRange range) const {
  if (!range.valid()) {
    return TabGroupRange::InvalidRange();
  }
  return absl::visit(
      [range](const auto& storage) { return storage.RangeAfterRemoval(range); },
      removing_);
}

RemovingIndexes::Storage RemovingIndexes::StorageFromRange(Range range) {
  if (range.count <= 0) {
    return EmptyStorage();
  }

  if (range.count == 1) {
    return OneIndexStorage(range.start);
  }

  return RangeStorage(range);
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
    return RangeStorage(Range{.start = start, .count = count});
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
