// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines a set of geometric ranges, and standard operations on it.

#ifndef PDF_LOADER_RANGE_SET_H_
#define PDF_LOADER_RANGE_SET_H_

#include <ostream>
#include <set>
#include <string>

#include "ui/gfx/range/range.h"

namespace chrome_pdf {

class RangeSet {
 public:
  RangeSet();
  explicit RangeSet(const gfx::Range& range);
  ~RangeSet();

  RangeSet(const RangeSet& range_set);
  RangeSet(RangeSet&& range_set);
  RangeSet& operator=(const RangeSet& other);

  bool operator==(const RangeSet& other) const;
  bool operator!=(const RangeSet& other) const;

  bool Contains(uint32_t point) const;
  bool Contains(const gfx::Range& range) const;
  bool Contains(const RangeSet& range_set) const;

  bool Intersects(const gfx::Range& range) const;
  bool Intersects(const RangeSet& range_set) const;

  void Union(const gfx::Range& range);
  void Union(const RangeSet& range_set);

  void Intersect(const gfx::Range& range);
  void Intersect(const RangeSet& range_set);

  void Subtract(const gfx::Range& range);
  void Subtract(const RangeSet& range_set);

  void Xor(const gfx::Range& range);
  void Xor(const RangeSet& range_set);

  bool IsEmpty() const;
  void Clear();

  gfx::Range First() const;
  gfx::Range Last() const;
  std::string ToString() const;

  struct range_compare {
    bool operator()(const gfx::Range& lval, const gfx::Range& rval) const {
      return lval.start() < rval.start();
    }
  };

  using RangesContainer = std::set<gfx::Range, range_compare>;

  const RangesContainer& ranges() const { return ranges_; }
  size_t Size() const { return ranges_.size(); }

 private:
  RangesContainer ranges_;
};

}  // namespace chrome_pdf

std::ostream& operator<<(std::ostream& os,
                         const chrome_pdf::RangeSet& range_set);

#endif  // PDF_LOADER_RANGE_SET_H_
