// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FIRST_PARTY_SETS_ADDITION_OVERLAPS_UNION_FIND_H_
#define NET_FIRST_PARTY_SETS_ADDITION_OVERLAPS_UNION_FIND_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "net/base/net_export.h"

namespace net {

// A helper class defining a Union-Find data structure that's used for merging
// disjoint transitively-overlapping sets together.
class NET_EXPORT AdditionOverlapsUnionFind {
 public:
  using SetsMap = base::flat_map<size_t, base::flat_set<size_t>>;

  // The number of sets (num_sets) must be greater than or equal to zero.
  explicit AdditionOverlapsUnionFind(int num_sets);
  ~AdditionOverlapsUnionFind();

  AdditionOverlapsUnionFind(const AdditionOverlapsUnionFind&) = delete;
  AdditionOverlapsUnionFind& operator=(const AdditionOverlapsUnionFind&) =
      delete;

  // Unions the two given sets together if they are in disjoint sets, and does
  // nothing if they are non-disjoint.
  // Unions are non-commutative for First-Party Sets; this method always chooses
  // the set with the lesser index as the primary.
  // Both set indices (set_x, set_y) must be in the range [0, num_sets) where
  // num_sets is the argument given to the constructor.
  // If Union is called when num_sets = 0, then this will crash.
  void Union(size_t set_x, size_t set_y);

  // Returns a mapping from an addition set index 'i' to a set of indices
  // which all have 'i' as their representative.
  SetsMap SetsMapping();

 private:
  // Returns the index for the representative of the given set.
  size_t Find(size_t set);

  std::vector<size_t> representatives_;
};

}  // namespace net

#endif  // NET_FIRST_PARTY_SETS_ADDITION_OVERLAPS_UNION_FIND_H_
