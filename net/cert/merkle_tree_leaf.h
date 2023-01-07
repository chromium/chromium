// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_MERKLE_TREE_LEAF_H_
#define NET_CERT_MERKLE_TREE_LEAF_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cert/signed_certificate_timestamp.h"

namespace net {

class X509Certificate;

namespace ct {

// Represents a MerkleTreeLeaf as defined in RFC6962, section 3.4.
// The goal of this struct is to represent the Merkle tree entry such that
// all details are easily accessible and a leaf hash can be easily calculated
// for the entry.
//
// As such, it has all the data as the MerkleTreeLeaf defined in the RFC,
// but it is not identical to the structure in the RFC for the following
// reasons:
// * The version is implicit - it is only used for V1 leaves currently.
// * the leaf_type is also implicit: There's exactly one leaf type and no
// new types are planned.
// * The timestamped_entry's |timestamp| and |extensions| fields are directly
// accessible.
// * The timestamped_entry's entry_type can be deduced from |signed_entry|.type
struct NET_EXPORT MerkleTreeLeaf {
  MerkleTreeLeaf();
  MerkleTreeLeaf(const MerkleTreeLeaf& other);
  MerkleTreeLeaf(MerkleTreeLeaf&&);
  ~MerkleTreeLeaf();

  // Certificate / Precertificate and indication of entry type.
  SignedEntryData signed_entry;

  // Timestamp from the SCT.
  base::Time timestamp;

  // Extensions from the SCT.
  std::string extensions;
};

// Given a |cert| and an |sct| for that certificate, constructs the
// representation of this entry in the Merkle tree by filling in
// |merkle_tree_leaf|.
// Returns false if it failed to construct the |merkle_tree_leaf|.
NET_EXPORT bool GetMerkleTreeLeaf(const X509Certificate* cert,
                                  const SignedCertificateTimestamp* sct,
                                  MerkleTreeLeaf* merkle_tree_leaf);

// Sets |*out| to the hash of the Merkle |tree_leaf|, as defined in RFC6962,
// section 3.4. Returns true if the hash was generated, false if an error
// occurred.
NET_EXPORT bool HashMerkleTreeLeaf(const MerkleTreeLeaf& tree_leaf,
                                   std::string* out);

}  // namespace ct

}  // namespace net

#endif  // NET_CERT_MERKLE_TREE_LEAF_H_
