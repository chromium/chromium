// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_hash_tree.h"

#include <memory>

#include "base/check_op.h"
#include "base/strings/string_view_util.h"
#include "crypto/hash.h"

namespace extensions {

std::string ComputeTreeHashRoot(const std::vector<std::string>& leaf_hashes,
                                int branch_factor) {
  if (leaf_hashes.empty() || branch_factor < 2) {
    return std::string();
  }

  // The nodes of the tree we're currently operating on.
  std::vector<std::string> current_nodes;

  // We avoid having to copy all of the input leaf nodes into |current_nodes|
  // by using a pointer. So the first iteration of the loop this points at
  // |leaf_hashes|, but thereafter it points at |current_nodes|.
  const std::vector<std::string>* current = &leaf_hashes;

  // Where we're inserting new hashes computed from the current level.
  std::vector<std::string> parent_nodes;

  while (current->size() > 1) {
    // Iterate over the current level of hashes, computing the hash of up to
    // |branch_factor| elements to form the hash of each parent node.
    auto i = current->cbegin();
    while (i != current->cend()) {
      crypto::hash::Hasher hash(crypto::hash::kSha256);
      for (int j = 0; j < branch_factor && i != current->end(); j++) {
        DCHECK_EQ(i->size(), crypto::hash::kSha256Size);
        hash.Update(*i);
        ++i;
      }
      std::string digest(crypto::hash::kSha256Size, '\0');
      hash.Finish(base::as_writable_byte_span(digest));
      parent_nodes.push_back(digest);
    }
    current_nodes.swap(parent_nodes);
    parent_nodes.clear();
    current = &current_nodes;
  }
  DCHECK_EQ(1u, current->size());
  return (*current)[0];
}

}  // namespace extensions
