// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_MERKLE_CONSISTENCY_PROOF_H_
#define NET_CERT_MERKLE_CONSISTENCY_PROOF_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "net/base/net_export.h"

namespace net::ct {

// Consistency proof between two STHs as defined in section 2.1.2. of RFC6962.
struct NET_EXPORT MerkleConsistencyProof {
  MerkleConsistencyProof();
  MerkleConsistencyProof(const std::string& log_id,
                         const std::vector<std::string>& proof_nodes,
                         uint64_t old_size,
                         uint64_t new_size);
  ~MerkleConsistencyProof();

  // The origin of this proof.
  std::string log_id;

  // Consistency proof nodes.
  std::vector<std::string> nodes;

  // Size of the older tree.
  uint64_t first_tree_size = 0;

  // Size of the newer tree.
  uint64_t second_tree_size = 0;
};

}  // namespace net::ct

#endif  // NET_CERT_MERKLE_CONSISTENCY_PROOF_H_
