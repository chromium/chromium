// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/merkle_audit_proof.h"

#include "base/check_op.h"

namespace net::ct {

uint64_t CalculateAuditPathLength(uint64_t leaf_index, uint64_t tree_size) {
  // RFC6962, section 2.1.1, describes audit paths.
  // Algorithm taken from
  // https://github.com/google/certificate-transparency-rfcs/blob/c8844de6bd0b5d3d16bac79865e6edef533d760b/dns/draft-ct-over-dns.md#retrieve-merkle-audit-proof-from-log-by-leaf-hash.
  CHECK_LT(leaf_index, tree_size);
  uint64_t length = 0;
  uint64_t index = leaf_index;
  uint64_t last_node = tree_size - 1;

  while (last_node != 0) {
    if ((index % 2 != 0) || index != last_node)
      ++length;
    index /= 2;
    last_node /= 2;
  }

  return length;
}

MerkleAuditProof::MerkleAuditProof() = default;

MerkleAuditProof::MerkleAuditProof(const MerkleAuditProof& other) = default;

MerkleAuditProof::MerkleAuditProof(uint64_t leaf_index,
                                   uint64_t tree_size,
                                   const std::vector<std::string>& audit_path)
    : leaf_index(leaf_index), tree_size(tree_size), nodes(audit_path) {}

MerkleAuditProof::~MerkleAuditProof() = default;

}  // namespace net::ct
