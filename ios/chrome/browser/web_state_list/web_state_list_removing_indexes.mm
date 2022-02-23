// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_list_removing_indexes.h"

#include <algorithm>

#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Constructs a WebStateListRemovingIndexes::Storage from a
// std::vector<int>, sorting it and removing duplicates.
WebStateListRemovingIndexes::Storage StorageFromVector(
    std::vector<int> indexes) {
  std::sort(indexes.begin(), indexes.end());
  indexes.erase(std::unique(indexes.begin(), indexes.end()), indexes.end());

  if (indexes.empty())
    return WebStateListRemovingIndexes::Empty{};

  if (indexes.size() == 1)
    return indexes[0];

  return indexes;
}

// Constructs a WebStateListRemovingIndexes::Storage from a
// std::initializer_list<int>.
WebStateListRemovingIndexes::Storage StorageFromInitializerList(
    std::initializer_list<int> indexes) {
  if (indexes.size() == 0)
    return WebStateListRemovingIndexes::Empty{};

  if (indexes.size() == 1)
    return *indexes.begin();

  // Use the vector overload.
  return StorageFromVector(std::vector<int>(std::move(indexes)));
}

// Visitor implementing WebStateListRemovingIndexes::count().
struct CountVisitor {
  using Empty = WebStateListRemovingIndexes::Empty;

  int operator()(const Empty&) const { return 0; }

  int operator()(const int& index) const { return 1; }

  int operator()(const std::vector<int>& indexes) const {
    return static_cast<int>(indexes.size());
  }
};

// Visitor implementing WebStateListRemovingIndexes::Contains(int).
struct ContainsVisitor {
  using Empty = WebStateListRemovingIndexes::Empty;

  explicit ContainsVisitor(int index) : index_(index) {}

  bool operator()(const Empty&) const { return false; }

  bool operator()(const int& index) const { return index_ == index; }

  bool operator()(const std::vector<int>& indexes) const {
    return std::binary_search(indexes.begin(), indexes.end(), index_);
  }

  const int index_;
};

// Visitor implementing WebStateListRemovingIndexes::IndexAfterRemoval(int).
struct IndexAfterRemovalVisitor {
  using Empty = WebStateListRemovingIndexes::Empty;

  explicit IndexAfterRemovalVisitor(int index) : index_(index) {}

  int operator()(const Empty&) const { return index_; }

  int operator()(const int& index) const {
    if (index_ == index)
      return WebStateList::kInvalidIndex;

    if (index_ > index)
      return index_ - 1;

    return index_;
  }

  int operator()(const std::vector<int>& indexes) const {
    const auto lower_bound =
        std::lower_bound(indexes.begin(), indexes.end(), index_);

    if (lower_bound == indexes.end() || *lower_bound != index_)
      return index_ - std::distance(indexes.begin(), lower_bound);

    return WebStateList::kInvalidIndex;
  }

  const int index_;
};

}  // anonymous namespace

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
  return absl::visit(CountVisitor(), removing_);
}

bool WebStateListRemovingIndexes::Contains(int index) const {
  return absl::visit(ContainsVisitor(index), removing_);
}

int WebStateListRemovingIndexes::IndexAfterRemoval(int index) const {
  return absl::visit(IndexAfterRemovalVisitor(index), removing_);
}
