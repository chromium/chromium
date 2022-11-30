// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_MERKLE_AUDIT_PROOF_H_
#define NET_CERT_MERKLE_AUDIT_PROOF_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "net/base/net_export.h"

namespace net::ct {

// Returns the length of the audit path for a leaf at |leaf_index| in a Merkle
// tree containing |tree_size| leaves.
// The |leaf_index| must be less than the |tree_size|.
NET_EXPORT uint64_t CalculateAuditPathLength(uint64_t leaf_index,
                                             uint64_t tree_size);

// Audit proof for a Merkle tree leaf, as defined in section 2.1.1. of RFC6962.
struct NET_EXPORT MerkleAuditProof {
  MerkleAuditProof();
  MerkleAuditProof(const MerkleAuditProof& other);
  MerkleAuditProof(uint64_t leaf_index,
                   uint64_t tree_size,
                   const std::vector<std::string>& audit_path);
  ~MerkleAuditProof();

  // Index of the tree leaf in the log.
  // Must be provided when fetching the proof from the log.
  uint64_t leaf_index = 0;

  // The proof works only in conjunction with an STH for this tree size.
  // Must be provided when fetching the proof from the log.
  uint64_t tree_size = 0;

  // Audit path nodes.
  // Using the leaf hash and these nodes, the STH hash can be reconstructed to
  // prove that leaf was included in the log's tree.
  std::vector<std::string> nodes;
};

}  // namespace net::ct

#endif  // NET_CERT_MERKLE_AUDIT_PROOF_H_
