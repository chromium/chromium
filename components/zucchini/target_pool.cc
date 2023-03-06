// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/target_pool.h"

#include <iterator>
#include <utility>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "components/zucchini/algorithm.h"
#include "components/zucchini/equivalence_map.h"

namespace zucchini {

TargetPool::TargetPool() = default;

TargetPool::TargetPool(std::deque<offset_t>&& targets) {
  DCHECK(targets_.empty());
  DCHECK(std::is_sorted(targets.begin(), targets.end()));
  targets_ = std::move(targets);
}

TargetPool::TargetPool(TargetPool&&) = default;
TargetPool::TargetPool(const TargetPool&) = default;
TargetPool::~TargetPool() = default;

void TargetPool::InsertTargets(const std::vector<offset_t>& targets) {
  base::ranges::copy(targets, std::back_inserter(targets_));
  SortAndUniquify(&targets_);
}

void TargetPool::InsertTargets(TargetSource* targets) {
  for (auto target = targets->GetNext(); target.has_value();
       target = targets->GetNext()) {
    targets_.push_back(*target);
  }
  // InsertTargets() can be called many times (number of reference types for the
  // pool) in succession. Calling SortAndUniquify() every time enables deduping
  // to occur more often. This prioritizes peak memory reduction over running
  // time.
  SortAndUniquify(&targets_);
}

void TargetPool::InsertTargets(const std::vector<Reference>& references) {
  // This can be called many times, so it's better to let std::back_inserter()
  // manage |targets_| resize, instead of manually reserving space.
  base::ranges::transform(references, std::back_inserter(targets_),
                          &Reference::target);
  SortAndUniquify(&targets_);
}

void TargetPool::InsertTargets(ReferenceReader&& references) {
  for (auto ref = references.GetNext(); ref.has_value();
       ref = references.GetNext()) {
    targets_.push_back(ref->target);
  }
  SortAndUniquify(&targets_);
}

key_t TargetPool::KeyForOffset(offset_t offset) const {
  auto pos = std::lower_bound(targets_.begin(), targets_.end(), offset);
  DCHECK(pos != targets_.end() && *pos == offset);
  return static_cast<offset_t>(pos - targets_.begin());
}

key_t TargetPool::KeyForNearestOffset(offset_t offset) const {
  auto pos = std::lower_bound(targets_.begin(), targets_.end(), offset);
  if (pos != targets_.begin()) {
    // If distances are equal, prefer lower key.
    if (pos == targets_.end() || *pos - offset >= offset - pos[-1])
      --pos;
  }
  return static_cast<offset_t>(pos - targets_.begin());
}

void TargetPool::FilterAndProject(const OffsetMapper& offset_mapper) {
  offset_mapper.ForwardProjectAll(&targets_);
  std::sort(targets_.begin(), targets_.end());
}

}  // namespace zucchini
