// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/addition_overlaps_union_find.h"

#include <numeric>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"

namespace net {

AdditionOverlapsUnionFind::AdditionOverlapsUnionFind(int num_sets) {
  CHECK_GE(num_sets, 0);
  representatives_.resize(num_sets);
  std::iota(representatives_.begin(), representatives_.end(), 0ul);
}

AdditionOverlapsUnionFind::~AdditionOverlapsUnionFind() = default;

void AdditionOverlapsUnionFind::Union(size_t set_x, size_t set_y) {
  CHECK_GE(set_x, 0ul);
  CHECK_LT(set_x, representatives_.size());
  CHECK_GE(set_y, 0ul);
  CHECK_LT(set_y, representatives_.size());

  size_t root_x = Find(set_x);
  size_t root_y = Find(set_y);

  if (root_x == root_y)
    return;
  auto [parent, child] = std::minmax(root_x, root_y);
  representatives_[child] = parent;
}

AdditionOverlapsUnionFind::SetsMap AdditionOverlapsUnionFind::SetsMapping() {
  SetsMap sets;

  // An insert into the flat_map and flat_set has O(n) complexity and
  // populating sets this way will be O(n^2).
  // This can be improved by creating an intermediate vector of pairs, each
  // representing an entry in sets, and then constructing the map all at once.
  // The intermediate vector stores pairs, using O(1) Insert. Another vector
  // the size of |num_sets| will have to be used for O(1) Lookup into the
  // first vector. This means making the intermediate vector will be O(n).
  // After the intermediate vector is populated, and we can use
  // base::MakeFlatMap to construct the mapping all at once.
  // This improvement makes this method less straightforward however.
  for (size_t i = 0; i < representatives_.size(); i++) {
    size_t cur_rep = Find(i);
    auto it = sets.emplace(cur_rep, base::flat_set<size_t>()).first;
    if (i != cur_rep) {
      it->second.insert(i);
    }
  }
  return sets;
}

size_t AdditionOverlapsUnionFind::Find(size_t set) {
  CHECK_GE(set, 0ul);
  CHECK_LT(set, representatives_.size());
  if (representatives_[set] != set)
    representatives_[set] = Find(representatives_[set]);
  return representatives_[set];
}

}  // namespace net
