// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/merkle_tree_leaf.h"

#include "crypto/sha2.h"
#include "net/cert/ct_objects_extractor.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/x509_certificate.h"

namespace net::ct {

MerkleTreeLeaf::MerkleTreeLeaf() = default;

MerkleTreeLeaf::MerkleTreeLeaf(const MerkleTreeLeaf& other) = default;

MerkleTreeLeaf::MerkleTreeLeaf(MerkleTreeLeaf&&) = default;

MerkleTreeLeaf::~MerkleTreeLeaf() = default;

bool HashMerkleTreeLeaf(const MerkleTreeLeaf& tree_leaf, std::string* out) {
  // Prepend 0 byte as per RFC 6962, section-2.1
  std::string leaf_in_tls_format("\x00", 1);
  if (!EncodeTreeLeaf(tree_leaf, &leaf_in_tls_format))
    return false;

  *out = crypto::SHA256HashString(leaf_in_tls_format);
  return true;
}

bool GetMerkleTreeLeaf(const X509Certificate* cert,
                       const SignedCertificateTimestamp* sct,
                       MerkleTreeLeaf* merkle_tree_leaf) {
  if (sct->origin == SignedCertificateTimestamp::SCT_EMBEDDED) {
    if (cert->intermediate_buffers().empty() ||
        !GetPrecertSignedEntry(cert->cert_buffer(),
                               cert->intermediate_buffers().front().get(),
                               &merkle_tree_leaf->signed_entry)) {
      return false;
    }
  } else {
    if (!GetX509SignedEntry(cert->cert_buffer(),
                            &merkle_tree_leaf->signed_entry)) {
      return false;
    }
  }

  merkle_tree_leaf->timestamp = sct->timestamp;
  merkle_tree_leaf->extensions = sct->extensions;
  return true;
}

}  // namespace net::ct
