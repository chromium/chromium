// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/loader/range_set.h"

#include <algorithm>
#include <sstream>
#include <vector>

namespace chrome_pdf {

namespace {

gfx::Range FixDirection(const gfx::Range& range) {
  if (!range.IsValid() || !range.is_reversed())
    return range;
  return gfx::Range(range.end() + 1, range.start() + 1);
}

}  // namespace

RangeSet::RangeSet() = default;

RangeSet::RangeSet(const gfx::Range& range) {
  Union(range);
}

RangeSet::RangeSet(const RangeSet& range_set) = default;

RangeSet::RangeSet(RangeSet&& range_set)
    : ranges_(std::move(range_set.ranges_)) {}

RangeSet& RangeSet::operator=(const RangeSet& other) = default;

RangeSet::~RangeSet() = default;

bool RangeSet::operator==(const RangeSet& other) const {
  return other.ranges_ == ranges_;
}

bool RangeSet::operator!=(const RangeSet& other) const {
  return other.ranges_ != ranges_;
}

void RangeSet::Union(const gfx::Range& range) {
  if (range.is_empty())
    return;
  gfx::Range fixed_range = FixDirection(range);
  if (IsEmpty()) {
    ranges_.insert(fixed_range);
    return;
  }

  auto start = ranges_.upper_bound(fixed_range);
  if (start != ranges_.begin())
    --start;  // start now points to the key equal or lower than offset.
  if (start->end() < fixed_range.start())
    ++start;  // start element is entirely before current range, skip it.

  auto end = ranges_.upper_bound(gfx::Range(fixed_range.end()));
  if (start == end) {  // No ranges to merge.
    ranges_.insert(fixed_range);
    return;
  }

  --end;

  int new_start = std::min<size_t>(start->start(), fixed_range.start());
  int new_end = std::max(end->end(), fixed_range.end());

  ranges_.erase(start, ++end);
  ranges_.insert(gfx::Range(new_start, new_end));
}

void RangeSet::Union(const RangeSet& range_set) {
  if (&range_set == this)
    return;
  for (const auto& it : range_set.ranges()) {
    Union(it);
  }
}

bool RangeSet::Contains(uint32_t point) const {
  return Contains(gfx::Range(point, point + 1));
}

bool RangeSet::Contains(const gfx::Range& range) const {
  if (range.is_empty())
    return false;
  const gfx::Range fixed_range = FixDirection(range);
  auto it = ranges().upper_bound(fixed_range);
  if (it == ranges().begin())
    return false;  // No ranges includes range.start().

  --it;  // Now it starts equal or before range.start().
  return it->end() >= fixed_range.end();
}

bool RangeSet::Contains(const RangeSet& range_set) const {
  for (const auto& it : range_set.ranges()) {
    if (!Contains(it))
      return false;
  }
  return true;
}

bool RangeSet::Intersects(const gfx::Range& range) const {
  if (IsEmpty() || range.is_empty())
    return false;
  const gfx::Range fixed_range = FixDirection(range);
  auto start = ranges_.upper_bound(fixed_range);
  if (start != ranges_.begin()) {
    --start;
  }
  // start now points to the key equal or lower than range.start().
  if (start->end() < range.start()) {
    // start element is entirely before current range, skip it.
    ++start;
  }
  auto end = ranges_.upper_bound(gfx::Range(fixed_range.end()));
  for (auto it = start; it != end; ++it) {
    if (fixed_range.end() > it->start() && fixed_range.start() < it->end())
      return true;
  }
  return false;
}

bool RangeSet::Intersects(const RangeSet& range_set) const {
  for (const auto& it : range_set.ranges()) {
    if (Intersects(it))
      return true;
  }
  return false;
}

void RangeSet::Intersect(const gfx::Range& range) {
  Intersect(RangeSet(range));
}

void RangeSet::Intersect(const RangeSet& range_set) {
  if (IsEmpty())
    return;
  RangesContainer new_ranges;
  for (const auto& range : range_set.ranges()) {
    auto start = ranges_.upper_bound(range);
    if (start != ranges_.begin())
      --start;  // start now points to the key equal or lower than
                // range.start().
    if (start->end() < range.start())
      ++start;  // start element is entirely before current range, skip it.
    auto end = ranges_.upper_bound(gfx::Range(range.end()));
    if (start == end) {  // No data in the current range available.
      continue;
    }
    for (auto it = start; it != end; ++it) {
      const gfx::Range new_range = range.Intersect(*it);
      if (!new_range.is_empty()) {
        new_ranges.insert(new_range);
      }
    }
  }
  new_ranges.swap(ranges_);
}

void RangeSet::Subtract(const gfx::Range& range) {
  if (range.is_empty() || IsEmpty())
    return;
  const gfx::Range fixed_range = FixDirection(range);
  auto start = ranges_.upper_bound(fixed_range);
  if (start != ranges_.begin())
    --start;  // start now points to the key equal or lower than
              // range.start().
  if (start->end() < fixed_range.start())
    ++start;  // start element is entirely before current range, skip it.
  auto end = ranges_.upper_bound(gfx::Range(fixed_range.end()));
  if (start == end) {  // No data in the current range available.
    return;
  }
  std::vector<gfx::Range> new_ranges;
  for (auto it = start; it != end; ++it) {
    const gfx::Range left(it->start(),
                          std::min(it->end(), fixed_range.start()));
    const gfx::Range right(std::max(it->start(), fixed_range.end()), it->end());
    if (!left.is_empty() && !left.is_reversed()) {
      new_ranges.push_back(left);
    }
    if (!right.is_empty() && !right.is_reversed() && right != left) {
      new_ranges.push_back(right);
    }
  }
  ranges_.erase(start, end);
  for (const auto& it : new_ranges) {
    ranges_.insert(it);
  }
}

void RangeSet::Subtract(const RangeSet& range_set) {
  if (&range_set == this) {
    ranges_.clear();
    return;
  }
  for (const auto& range : range_set.ranges()) {
    Subtract(range);
  }
}

void RangeSet::Xor(const gfx::Range& range) {
  Xor(RangeSet(range));
}

void RangeSet::Xor(const RangeSet& range_set) {
  RangeSet tmp = *this;
  tmp.Intersect(range_set);
  Union(range_set);
  Subtract(tmp);
}

bool RangeSet::IsEmpty() const {
  return ranges().empty();
}

void RangeSet::Clear() {
  ranges_.clear();
}

gfx::Range RangeSet::First() const {
  return *ranges().begin();
}

gfx::Range RangeSet::Last() const {
  return *ranges().rbegin();
}

std::string RangeSet::ToString() const {
  std::stringstream ss;
  ss << "{";
  for (const auto& it : ranges()) {
    ss << "[" << it.start() << "," << it.end() << ")";
  }
  ss << "}";
  return ss.str();
}

}  // namespace chrome_pdf

std::ostream& operator<<(std::ostream& os,
                         const chrome_pdf::RangeSet& range_set) {
  return (os << range_set.ToString());
}
