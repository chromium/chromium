// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/addition_overlaps_union_find.h"

#include <numeric>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"

namespace content {

AdditionOverlapsUnionFind::AdditionOverlapsUnionFind(int num_sets) {
  CHECK_GE(num_sets, 0);
  representatives_.resize(num_sets);
  std::iota(representatives_.begin(), representatives_.end(), 0);
}

AdditionOverlapsUnionFind::~AdditionOverlapsUnionFind() = default;

void AdditionOverlapsUnionFind::Union(int set_x, int set_y) {
  CHECK_GE(set_x, 0);
  CHECK_LT(set_x, static_cast<int>(representatives_.size()));
  CHECK_GE(set_y, 0);
  CHECK_LT(set_y, static_cast<int>(representatives_.size()));

  int root_x = Find(set_x);
  int root_y = Find(set_y);

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
  for (int i = 0; i < static_cast<int>(representatives_.size()); i++) {
    int cur_rep = Find(i);
    if (!sets.contains(cur_rep)) {
      sets.emplace(cur_rep, base::flat_set<int>());
    }
    if (i != cur_rep) {
      base::flat_set<int>& overlaps = sets.at(cur_rep);
      overlaps.insert(i);
    }
  }
  return sets;
}

int AdditionOverlapsUnionFind::Find(int set) {
  CHECK_GE(set, 0);
  CHECK_LT(set, static_cast<int>(representatives_.size()));
  if (representatives_[set] != set)
    representatives_[set] = Find(representatives_[set]);
  return representatives_[set];
}

}  // namespace content
