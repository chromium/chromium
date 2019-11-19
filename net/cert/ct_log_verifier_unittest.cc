// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_log_verifier.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "crypto/secure_hash.h"
#include "net/base/hash_value.h"
#include "net/cert/ct_log_verifier_util.h"
#include "net/cert/merkle_audit_proof.h"
#include "net/cert/merkle_consistency_proof.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_tree_head.h"
#include "net/test/ct_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Calculate the power of two nearest to, but less than, |n|.
// |n| must be at least 2.
size_t CalculateNearestPowerOfTwo(size_t n) {
  DCHECK_GT(n, 1u);

  size_t ret = size_t(1) << (sizeof(size_t) * 8 - 1);
  while (ret >= n)
    ret >>= 1;

  return ret;
}

// All test data replicated from
// https://github.com/google/certificate-transparency/blob/c41b090ecc14ddd6b3531dc7e5ce36b21e253fdd/cpp/merkletree/merkle_tree_test.cc

// The SHA-256 hash of an empty Merkle tree.
const uint8_t kEmptyTreeHash[32] = {
    0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4,
    0xc8, 0x99, 0x6f, 0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b,
    0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55};

std::string GetEmptyTreeHash() {
  return std::string(std::begin(kEmptyTreeHash), std::end(kEmptyTreeHash));
}

// SHA-256 Merkle leaf hashes for the sample tree that all of the other test
// data relates to (8 leaves).
const char* const kLeafHashes[8] = {
    "6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d",
    "96a296d224f285c67bee93c30f8a309157f0daa35dc5b87e410b78630a09cfc7",
    "0298d122906dcfc10892cb53a73992fc5b9f493ea4c9badb27b791b4127a7fe7",
    "07506a85fd9dd2f120eb694f86011e5bb4662e5c415a62917033d4a9624487e7",
    "bc1a0643b12e4d2d7c77918f44e0f4f79a838b6cf9ec5b5c283e1f4d88599e6b",
    "4271a26be0d8a84f0bd54c8c302e7cb3a3b5d1fa6780a40bcce2873477dab658",
    "b08693ec2e721597130641e8211e7eedccb4c26413963eee6c1e2ed16ffb1a5f",
    "46f6ffadd3d06a09ff3c5860d2755c8b9819db7df44251788c7d8e3180de8eb1"};

// SHA-256 Merkle root hashes from building the sample tree leaf-by-leaf.
// The first entry is the root when the tree contains 1 leaf, and the last is
// the root when the tree contains all 8 leaves.
const char* const kRootHashes[8] = {
    "6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d",
    "fac54203e7cc696cf0dfcb42c92a1d9dbaf70ad9e621f4bd8d98662f00e3c125",
    "aeb6bcfe274b70a14fb067a5e5578264db0fa9b51af5e0ba159158f329e06e77",
    "d37ee418976dd95753c1c73862b9398fa2a2cf9b4ff0fdfe8b30cd95209614b7",
    "4e3bbb1f7b478dcfe71fb631631519a3bca12c9aefca1612bfce4c13a86264d4",
    "76e67dadbcdf1e10e1b74ddc608abd2f98dfb16fbce75277b5232a127f2087ef",
    "ddb89be403809e325750d3d263cd78929c2942b7942a34b77e122c9594a74c8c",
    "5dc9da79a70659a9ad559cb701ded9a2ab9d823aad2f4960cfe370eff4604328"};

// A single consistency proof. Contains at most 3 proof nodes (all test proofs
// will be for a tree of size 8).
struct ConsistencyProofTestVector {
  size_t old_tree_size;
  size_t new_tree_size;
  size_t proof_length;
  const char* const proof[3];
};

// A collection of consistency proofs between various sub-trees of the sample
// tree.
const ConsistencyProofTestVector kConsistencyProofs[] = {
    // Empty consistency proof between trees of the same size (1).
    {1, 1, 0, {"", "", ""}},
    // Consistency proof between tree of size 1 and tree of size 8, with 3
    // nodes in the proof.
    {1,
     8,
     3,
     {"96a296d224f285c67bee93c30f8a309157f0daa35dc5b87e410b78630a09cfc7",
      "5f083f0a1a33ca076a95279832580db3e0ef4584bdff1f54c8a360f50de3031e",
      "6b47aaf29ee3c2af9af889bc1fb9254dabd31177f16232dd6aab035ca39bf6e4"}},
    // Consistency proof between tree of size 6 and tree of size 8, with 3
    // nodes in the proof.
    {6,
     8,
     3,
     {"0ebc5d3437fbe2db158b9f126a1d118e308181031d0a949f8dededebc558ef6a",
      "ca854ea128ed050b41b35ffc1b87b8eb2bde461e9e3b5596ece6b9d5975a0ae0",
      "d37ee418976dd95753c1c73862b9398fa2a2cf9b4ff0fdfe8b30cd95209614b7"}},
    // Consistency proof between tree of size 2 and tree of size 5, with 2
    // nodes in the proof.
    {2,
     5,
     2,
     {"5f083f0a1a33ca076a95279832580db3e0ef4584bdff1f54c8a360f50de3031e",
      "bc1a0643b12e4d2d7c77918f44e0f4f79a838b6cf9ec5b5c283e1f4d88599e6b", ""}}};

// A single audit proof. Contains at most 3 proof nodes (all test proofs will be
// for a tree of size 8).
struct AuditProofTestVector {
  size_t leaf;
  size_t tree_size;
  size_t proof_length;
  const char* const proof[3];
};

// A collection of audit proofs for various leaves and sub-trees of the tree
// defined by |kRootHashes|.
const AuditProofTestVector kAuditProofs[] = {
    {0, 1, 0, {"", "", ""}},
    {0,
     8,
     3,
     {"96a296d224f285c67bee93c30f8a309157f0daa35dc5b87e410b78630a09cfc7",
      "5f083f0a1a33ca076a95279832580db3e0ef4584bdff1f54c8a360f50de3031e",
      "6b47aaf29ee3c2af9af889bc1fb9254dabd31177f16232dd6aab035ca39bf6e4"}},
    {5,
     8,
     3,
     {"bc1a0643b12e4d2d7c77918f44e0f4f79a838b6cf9ec5b5c283e1f4d88599e6b",
      "ca854ea128ed050b41b35ffc1b87b8eb2bde461e9e3b5596ece6b9d5975a0ae0",
      "d37ee418976dd95753c1c73862b9398fa2a2cf9b4ff0fdfe8b30cd95209614b7"}},
    {2,
     3,
     1,
     {"fac54203e7cc696cf0dfcb42c92a1d9dbaf70ad9e621f4bd8d98662f00e3c125", "",
      ""}},
    {1,
     5,
     3,
     {"6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d",
      "5f083f0a1a33ca076a95279832580db3e0ef4584bdff1f54c8a360f50de3031e",
      "bc1a0643b12e4d2d7c77918f44e0f4f79a838b6cf9ec5b5c283e1f4d88599e6b"}}};

// Decodes a hexadecimal string into the binary data it represents.
std::string HexToBytes(const std::string& hex_data) {
  std::string result;
  if (!base::HexStringToString(hex_data, &result))
    result.clear();
  return result;
}

// Constructs a consistency/audit proof from a test vector.
// This is templated so that it can be used with both ConsistencyProofTestVector
// and AuditProofTestVector.
template <typename TestVectorType>
std::vector<std::string> GetProof(const TestVectorType& test_vector) {
  std::vector<std::string> proof(test_vector.proof_length);
  std::transform(test_vector.proof,
                 test_vector.proof + test_vector.proof_length, proof.begin(),
                 &HexToBytes);

  return proof;
}

// Creates a ct::MerkleConsistencyProof from its arguments and returns the
// result of passing this to log.VerifyConsistencyProof().
bool VerifyConsistencyProof(const CTLogVerifier& log,
                            size_t old_tree_size,
                            const std::string& old_tree_root,
                            size_t new_tree_size,
                            const std::string& new_tree_root,
                            const std::vector<std::string>& proof) {
  return log.VerifyConsistencyProof(
      ct::MerkleConsistencyProof(log.key_id(), proof, old_tree_size,
                                 new_tree_size),
      old_tree_root, new_tree_root);
}

// Creates a ct::MerkleAuditProof from its arguments and returns the result of
// passing this to log.VerifyAuditProof().
bool VerifyAuditProof(const CTLogVerifier& log,
                      size_t leaf,
                      size_t tree_size,
                      const std::vector<std::string>& proof,
                      const std::string& tree_root,
                      const std::string& leaf_hash) {
  return log.VerifyAuditProof(ct::MerkleAuditProof(leaf, tree_size, proof),
                              tree_root, leaf_hash);
}

class CTLogVerifierTest : public ::testing::Test {
 public:
  void SetUp() override {
    log_ = CTLogVerifier::Create(ct::GetTestPublicKey(), "testlog");

    ASSERT_TRUE(log_);
    EXPECT_EQ(ct::GetTestPublicKeyId(), log_->key_id());
  }

 protected:
  scoped_refptr<const CTLogVerifier> log_;
};

// Given an audit proof for a leaf in a Merkle tree, asserts that it verifies
// and no other combination of leaves, tree sizes and proof nodes verifies.
void CheckVerifyAuditProof(const CTLogVerifier& log,
                           size_t leaf,
                           size_t tree_size,
                           const std::vector<std::string>& proof,
                           const std::string& root_hash,
                           const std::string& leaf_hash) {
  EXPECT_TRUE(
      VerifyAuditProof(log, leaf, tree_size, proof, root_hash, leaf_hash))
      << "proof for leaf " << leaf << " did not pass verification";
  EXPECT_FALSE(
      VerifyAuditProof(log, leaf - 1, tree_size, proof, root_hash, leaf_hash))
      << "proof passed verification with wrong leaf index";
  EXPECT_FALSE(
      VerifyAuditProof(log, leaf + 1, tree_size, proof, root_hash, leaf_hash))
      << "proof passed verification with wrong leaf index";
  EXPECT_FALSE(
      VerifyAuditProof(log, leaf ^ 2, tree_size, proof, root_hash, leaf_hash))
      << "proof passed verification with wrong leaf index";
  EXPECT_FALSE(
      VerifyAuditProof(log, leaf, tree_size * 2, proof, root_hash, leaf_hash))
      << "proof passed verification with wrong tree height";
  EXPECT_FALSE(VerifyAuditProof(log, leaf / 2, tree_size / 2, proof, root_hash,
                                leaf_hash))
      << "proof passed verification with wrong leaf index and tree height";
  EXPECT_FALSE(
      VerifyAuditProof(log, leaf, tree_size / 2, proof, root_hash, leaf_hash))
      << "proof passed verification with wrong tree height";
  EXPECT_FALSE(VerifyAuditProof(log, leaf, tree_size, proof, GetEmptyTreeHash(),
                                leaf_hash))
      << "proof passed verification with wrong root hash";

  std::vector<std::string> wrong_proof;

  // Modify a single element on the proof.
  for (size_t j = 0; j < proof.size(); ++j) {
    wrong_proof = proof;
    wrong_proof[j] = GetEmptyTreeHash();
    EXPECT_FALSE(VerifyAuditProof(log, leaf, tree_size, wrong_proof, root_hash,
                                  leaf_hash))
        << "proof passed verification with one wrong node (node " << j << ")";
  }

  wrong_proof = proof;
  wrong_proof.push_back(std::string());
  EXPECT_FALSE(
      VerifyAuditProof(log, leaf, tree_size, wrong_proof, root_hash, leaf_hash))
      << "proof passed verification with an empty node appended";

  wrong_proof.back() = root_hash;
  EXPECT_FALSE(
      VerifyAuditProof(log, leaf, tree_size, wrong_proof, root_hash, leaf_hash))
      << "proof passed verification with an incorrect node appended";
  wrong_proof.pop_back();

  if (!wrong_proof.empty()) {
    wrong_proof.pop_back();
    EXPECT_FALSE(VerifyAuditProof(log, leaf, tree_size, wrong_proof, root_hash,
                                  leaf_hash))
        << "proof passed verification with the last node missing";
  }

  wrong_proof.clear();
  wrong_proof.push_back(std::string());
  wrong_proof.insert(wrong_proof.end(), proof.begin(), proof.end());
  EXPECT_FALSE(
      VerifyAuditProof(log, leaf, tree_size, wrong_proof, root_hash, leaf_hash))
      << "proof passed verification with an empty node prepended";

  wrong_proof[0] = root_hash;
  EXPECT_FALSE(
      VerifyAuditProof(log, leaf, tree_size, wrong_proof, root_hash, leaf_hash))
      << "proof passed verification with an incorrect node prepended";
}

// Given a consistency proof between two snapshots of the tree, asserts that it
// verifies and no other combination of tree sizes and proof nodes verifies.
void CheckVerifyConsistencyProof(const CTLogVerifier& log,
                                 int old_tree_size,
                                 int new_tree_size,
                                 const std::string& old_root,
                                 const std::string& new_root,
                                 const std::vector<std::string>& proof) {
  // Verify the original consistency proof.
  EXPECT_TRUE(VerifyConsistencyProof(log, old_tree_size, old_root,
                                     new_tree_size, new_root, proof))
      << "proof between trees of size " << old_tree_size << " and "
      << new_tree_size << " did not pass verification";

  if (proof.empty()) {
    // For simplicity test only non-trivial proofs that have old_root !=
    // new_root
    // old_tree_size != 0 and old_tree_size != new_tree_size.
    return;
  }

  // Wrong tree size: The proof checking code should not accept as a valid proof
  // a proof for a tree size different than the original size it was produced
  // for. Test that this is not the case for off-by-one changes.
  EXPECT_FALSE(VerifyConsistencyProof(log, old_tree_size - 1, old_root,
                                      new_tree_size, new_root, proof))
      << "proof passed verification with old tree size - 1";
  EXPECT_FALSE(VerifyConsistencyProof(log, old_tree_size + 1, old_root,
                                      new_tree_size, new_root, proof))
      << "proof passed verification with old tree size + 1";
  EXPECT_FALSE(VerifyConsistencyProof(log, old_tree_size ^ 2, old_root,
                                      new_tree_size, new_root, proof))
      << "proof passed verification with old tree size ^ 2";

  EXPECT_FALSE(VerifyConsistencyProof(log, old_tree_size, old_root,
                                      new_tree_size * 2, new_root, proof))
      << "proof passed verification with new tree height + 1";
  EXPECT_FALSE(VerifyConsistencyProof(log, old_tree_size, old_root,
                                      new_tree_size / 2, new_root, proof))
      << "proof passed verification with new tree height - 1";

  const std::string wrong_root("WrongRoot");
  EXPECT_FALSE(VerifyConsistencyProof(log, old_tree_size, old_root,
                                      new_tree_size, wrong_root, proof))
      << "proof passed verification with wrong old root";
  EXPECT_FALSE(VerifyConsistencyProof(log, old_tree_size, wrong_root,
                                      new_tree_size, new_root, proof))
      << "proof passed verification with wrong new root";
  EXPECT_FALSE(VerifyConsistencyProof(log, old_tree_size, new_root,
                                      new_tree_size, old_root, proof))
      << "proof passed verification with old and new root swapped";

  // Variations of wrong proofs, all of which should be rejected.
  std::vector<std::string> wrong_proof;
  EXPECT_FALSE(VerifyConsistencyProof(log, old_tree_size, old_root,
                                      new_tree_size, new_root, wrong_proof))
      << "empty proof passed verification";

  // Modify a single element in the proof.
  for (size_t j = 0; j < proof.size(); ++j) {
    wrong_proof = proof;
    wrong_proof[j] = GetEmptyTreeHash();
    EXPECT_FALSE(VerifyConsistencyProof(log, old_tree_size, old_root,
                                        new_tree_size, new_root, wrong_proof))
        << "proof passed verification with incorrect node (node " << j << ")";
  }

  wrong_proof = proof;
  wrong_proof.push_back(std::string());
  EXPECT_FALSE(VerifyConsistencyProof(log, old_tree_size, old_root,
                                      new_tree_size, new_root, wrong_proof))
      << "proof passed verification with empty node appended";

  wrong_proof.back() = proof.back();
  EXPECT_FALSE(VerifyConsistencyProof(log, old_tree_size, old_root,
                                      new_tree_size, new_root, wrong_proof))
      << "proof passed verification with last node duplicated";
  wrong_proof.pop_back();

  wrong_proof.pop_back();
  EXPECT_FALSE(VerifyConsistencyProof(log, old_tree_size, old_root,
                                      new_tree_size, new_root, wrong_proof))
      << "proof passed verification with last node missing";

  wrong_proof.clear();
  wrong_proof.push_back(std::string());
  wrong_proof.insert(wrong_proof.end(), proof.begin(), proof.end());
  EXPECT_FALSE(VerifyConsistencyProof(log, old_tree_size, old_root,
                                      new_tree_size, new_root, wrong_proof))
      << "proof passed verification with empty node prepended";

  wrong_proof[0] = proof[0];
  EXPECT_FALSE(VerifyConsistencyProof(log, old_tree_size, old_root,
                                      new_tree_size, new_root, wrong_proof))
      << "proof passed verification with first node duplicated";
}

TEST_F(CTLogVerifierTest, VerifiesCertSCT) {
  ct::SignedEntryData cert_entry;
  ct::GetX509CertSignedEntry(&cert_entry);

  scoped_refptr<ct::SignedCertificateTimestamp> cert_sct;
  ct::GetX509CertSCT(&cert_sct);

  EXPECT_TRUE(log_->Verify(cert_entry, *cert_sct.get()));
}

TEST_F(CTLogVerifierTest, VerifiesPrecertSCT) {
  ct::SignedEntryData precert_entry;
  ct::GetPrecertSignedEntry(&precert_entry);

  scoped_refptr<ct::SignedCertificateTimestamp> precert_sct;
  ct::GetPrecertSCT(&precert_sct);

  EXPECT_TRUE(log_->Verify(precert_entry, *precert_sct.get()));
}

TEST_F(CTLogVerifierTest, FailsInvalidTimestamp) {
  ct::SignedEntryData cert_entry;
  ct::GetX509CertSignedEntry(&cert_entry);

  scoped_refptr<ct::SignedCertificateTimestamp> cert_sct;
  ct::GetX509CertSCT(&cert_sct);

  // Mangle the timestamp, so that it should fail signature validation.
  cert_sct->timestamp = base::Time::Now();

  EXPECT_FALSE(log_->Verify(cert_entry, *cert_sct.get()));
}

TEST_F(CTLogVerifierTest, FailsInvalidLogID) {
  ct::SignedEntryData cert_entry;
  ct::GetX509CertSignedEntry(&cert_entry);

  scoped_refptr<ct::SignedCertificateTimestamp> cert_sct;
  ct::GetX509CertSCT(&cert_sct);

  // Mangle the log ID, which should cause it to match a different log before
  // attempting signature validation.
  cert_sct->log_id.assign(cert_sct->log_id.size(), '\0');

  EXPECT_FALSE(log_->Verify(cert_entry, *cert_sct.get()));
}

TEST_F(CTLogVerifierTest, VerifiesValidSTH) {
  ct::SignedTreeHead sth;
  ASSERT_TRUE(ct::GetSampleSignedTreeHead(&sth));
  EXPECT_TRUE(log_->VerifySignedTreeHead(sth));
}

TEST_F(CTLogVerifierTest, DoesNotVerifyInvalidSTH) {
  ct::SignedTreeHead sth;
  ASSERT_TRUE(ct::GetSampleSignedTreeHead(&sth));
  sth.sha256_root_hash[0] = '\x0';
  EXPECT_FALSE(log_->VerifySignedTreeHead(sth));
}

TEST_F(CTLogVerifierTest, VerifiesValidEmptySTH) {
  ct::SignedTreeHead sth;
  ASSERT_TRUE(ct::GetSampleEmptySignedTreeHead(&sth));
  EXPECT_TRUE(log_->VerifySignedTreeHead(sth));
}

TEST_F(CTLogVerifierTest, DoesNotVerifyInvalidEmptySTH) {
  ct::SignedTreeHead sth;
  ASSERT_TRUE(ct::GetBadEmptySignedTreeHead(&sth));
  EXPECT_FALSE(log_->VerifySignedTreeHead(sth));
}

// Test that excess data after the public key is rejected.
TEST_F(CTLogVerifierTest, ExcessDataInPublicKey) {
  std::string key = ct::GetTestPublicKey();
  key += "extra";

  scoped_refptr<const CTLogVerifier> log =
      CTLogVerifier::Create(key, "testlog");
  EXPECT_FALSE(log);
}

TEST_F(CTLogVerifierTest, VerifiesConsistencyProofEdgeCases_EmptyProof) {
  std::vector<std::string> empty_proof;
  std::string old_root(GetEmptyTreeHash()), new_root(GetEmptyTreeHash());

  // Tree snapshots that are always consistent, because the proofs are either
  // from an empty tree to a non-empty one or for trees of the same size.
  EXPECT_TRUE(
      VerifyConsistencyProof(*log_, 0, old_root, 0, new_root, empty_proof));
  EXPECT_TRUE(
      VerifyConsistencyProof(*log_, 0, old_root, 1, new_root, empty_proof));
  EXPECT_TRUE(
      VerifyConsistencyProof(*log_, 1, old_root, 1, new_root, empty_proof));

  // Invalid consistency proofs.
  // Time travel to the past.
  EXPECT_FALSE(
      VerifyConsistencyProof(*log_, 1, old_root, 0, new_root, empty_proof));
  EXPECT_FALSE(
      VerifyConsistencyProof(*log_, 2, old_root, 1, new_root, empty_proof));
  // Proof between two trees of different size can never be empty.
  EXPECT_FALSE(
      VerifyConsistencyProof(*log_, 1, old_root, 2, new_root, empty_proof));
}

TEST_F(CTLogVerifierTest, VerifiesConsistencyProofEdgeCases_MismatchingRoots) {
  const std::string old_root(GetEmptyTreeHash());
  std::string new_root;
  std::vector<std::string> empty_proof;

  // Roots don't match.
  EXPECT_FALSE(
      VerifyConsistencyProof(*log_, 0, old_root, 0, new_root, empty_proof));
  EXPECT_FALSE(
      VerifyConsistencyProof(*log_, 1, old_root, 1, new_root, empty_proof));
}

TEST_F(CTLogVerifierTest,
       VerifiesConsistencyProofEdgeCases_MatchingRootsNonEmptyProof) {
  const std::string empty_tree_hash(GetEmptyTreeHash());

  std::vector<std::string> proof;
  proof.push_back(empty_tree_hash);

  // Roots match and the tree size is either the same or the old tree size is 0,
  // but the proof is not empty (the verification code should not accept
  // proofs with redundant nodes in this case).
  proof.push_back(empty_tree_hash);
  EXPECT_FALSE(VerifyConsistencyProof(*log_, 0, empty_tree_hash, 0,
                                      empty_tree_hash, proof));
  EXPECT_FALSE(VerifyConsistencyProof(*log_, 0, empty_tree_hash, 1,
                                      empty_tree_hash, proof));
  EXPECT_FALSE(VerifyConsistencyProof(*log_, 1, empty_tree_hash, 1,
                                      empty_tree_hash, proof));
}

class CTLogVerifierConsistencyProofTest
    : public CTLogVerifierTest,
      public ::testing::WithParamInterface<size_t /* proof index */> {};

// Checks that a sample set of valid consistency proofs verify successfully.
TEST_P(CTLogVerifierConsistencyProofTest, VerifiesValidConsistencyProof) {
  const ConsistencyProofTestVector& test_vector =
      kConsistencyProofs[GetParam()];
  const std::vector<std::string> proof = GetProof(test_vector);

  const char* const old_root = kRootHashes[test_vector.old_tree_size - 1];
  const char* const new_root = kRootHashes[test_vector.new_tree_size - 1];
  CheckVerifyConsistencyProof(*log_, test_vector.old_tree_size,
                              test_vector.new_tree_size, HexToBytes(old_root),
                              HexToBytes(new_root), proof);
}

INSTANTIATE_TEST_SUITE_P(KnownGoodProofs,
                         CTLogVerifierConsistencyProofTest,
                         ::testing::Range(size_t(0),
                                          base::size(kConsistencyProofs)));

class CTLogVerifierAuditProofTest
    : public CTLogVerifierTest,
      public ::testing::WithParamInterface<size_t /* proof index */> {};

// Checks that a sample set of valid audit proofs verify successfully.
TEST_P(CTLogVerifierAuditProofTest, VerifiesValidAuditProofs) {
  const AuditProofTestVector& test_vector = kAuditProofs[GetParam()];
  const std::vector<std::string> proof = GetProof(test_vector);

  const char* const root_hash = kRootHashes[test_vector.tree_size - 1];
  CheckVerifyAuditProof(*log_, test_vector.leaf, test_vector.tree_size, proof,
                        HexToBytes(root_hash),
                        HexToBytes(kLeafHashes[test_vector.leaf]));
}

INSTANTIATE_TEST_SUITE_P(KnownGoodProofs,
                         CTLogVerifierAuditProofTest,
                         ::testing::Range(size_t(0), base::size(kAuditProofs)));

TEST_F(CTLogVerifierTest, VerifiesAuditProofEdgeCases_InvalidLeafIndex) {
  std::vector<std::string> proof;
  EXPECT_FALSE(
      VerifyAuditProof(*log_, 1, 0, proof, std::string(), std::string()));
  EXPECT_FALSE(
      VerifyAuditProof(*log_, 2, 1, proof, std::string(), std::string()));

  const std::string empty_hash = GetEmptyTreeHash();
  EXPECT_FALSE(VerifyAuditProof(*log_, 1, 0, proof, empty_hash, std::string()));
  EXPECT_FALSE(VerifyAuditProof(*log_, 2, 1, proof, empty_hash, std::string()));
}

// Functions that implement algorithms from RFC6962 necessary for constructing
// Merkle trees and proofs. This allows tests to generate a variety of trees
// for exhaustive testing.
namespace rfc6962 {

// Calculates the hash of a leaf in a Merkle tree, given its content.
// See RFC6962, section 2.1.
std::string HashLeaf(const std::string& leaf) {
  const char kLeafPrefix[] = {'\x00'};

  SHA256HashValue sha256;
  memset(sha256.data, 0, sizeof(sha256.data));

  std::unique_ptr<crypto::SecureHash> hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  hash->Update(kLeafPrefix, 1);
  hash->Update(leaf.data(), leaf.size());
  hash->Finish(sha256.data, sizeof(sha256.data));

  return std::string(reinterpret_cast<const char*>(sha256.data),
                     sizeof(sha256.data));
}

// Calculates the root hash of a Merkle tree, given its leaf data and size.
// See RFC6962, section 2.1.
std::string HashTree(std::string leaves[], size_t tree_size) {
  if (tree_size == 0)
    return GetEmptyTreeHash();
  if (tree_size == 1)
    return HashLeaf(leaves[0]);

  // Find the index of the last leaf in the left sub-tree.
  const size_t split = CalculateNearestPowerOfTwo(tree_size);

  // Hash the left and right sub-trees, then hash the results.
  return ct::internal::HashNodes(HashTree(leaves, split),
                                 HashTree(&leaves[split], tree_size - split));
}

// Returns a Merkle audit proof for the leaf with index |leaf_index|.
// The tree consists of |leaves[0]| to |leaves[tree_size-1]|.
// If |leaf_index| is >= |tree_size|, an empty proof will be returned.
// See RFC6962, section 2.1.1, for more details.
std::vector<std::string> CreateAuditProof(std::string leaves[],
                                          size_t tree_size,
                                          size_t leaf_index) {
  std::vector<std::string> proof;
  if (leaf_index >= tree_size)
    return proof;
  if (tree_size == 1)
    return proof;

  // Find the index of the first leaf in the right sub-tree.
  const size_t split = CalculateNearestPowerOfTwo(tree_size);

  // Recurse down the correct branch of the tree (left or right) to reach the
  // leaf with |leaf_index|. Add the hash of the branch not taken at each step
  // on the way up to build the proof.
  if (leaf_index < split) {
    proof = CreateAuditProof(leaves, split, leaf_index);
    proof.push_back(HashTree(&leaves[split], tree_size - split));
  } else {
    proof =
        CreateAuditProof(&leaves[split], tree_size - split, leaf_index - split);
    proof.push_back(HashTree(leaves, split));
  }

  return proof;
}

// Returns a Merkle consistency proof between two Merkle trees.
// The old tree contains |leaves[0]| to |leaves[old_tree_size-1]|.
// The new tree contains |leaves[0]| to |leaves[new_tree_size-1]|.
// Call with |contains_old_tree| = true.
// See RFC6962, section 2.1.2, for more details.
std::vector<std::string> CreateConsistencyProof(std::string leaves[],
                                                size_t new_tree_size,
                                                size_t old_tree_size,
                                                bool contains_old_tree = true) {
  std::vector<std::string> proof;
  if (old_tree_size == 0 || old_tree_size > new_tree_size)
    return proof;
  if (old_tree_size == new_tree_size) {
    // Consistency proof for two equal subtrees is empty.
    if (!contains_old_tree) {
      // Record the hash of this subtree unless it's the root for which
      // the proof was originally requested. (This happens when the old tree is
      // balanced).
      proof.push_back(HashTree(leaves, old_tree_size));
    }
    return proof;
  }

  // Find the index of the last leaf in the left sub-tree.
  const size_t split = CalculateNearestPowerOfTwo(new_tree_size);

  if (old_tree_size <= split) {
    // Root of the old tree is in the left subtree of the new tree.
    // Prove that the left subtrees are consistent.
    proof =
        CreateConsistencyProof(leaves, split, old_tree_size, contains_old_tree);
    // Record the hash of the right subtree (only present in the new tree).
    proof.push_back(HashTree(&leaves[split], new_tree_size - split));
  } else {
    // The old tree root is at the same level as the new tree root.
    // Prove that the right subtrees are consistent. The right subtree
    // doesn't contain the root of the old tree, so set contains_old_tree =
    // false.
    proof = CreateConsistencyProof(&leaves[split], new_tree_size - split,
                                   old_tree_size - split,
                                   /* contains_old_tree = */ false);
    // Record the hash of the left subtree (equal in both trees).
    proof.push_back(HashTree(leaves, split));
  }
  return proof;
}

}  // namespace rfc6962

class CTLogVerifierTestUsingGenerator
    : public CTLogVerifierTest,
      public ::testing::WithParamInterface<size_t /* tree_size */> {};

// Checks that valid consistency proofs for a range of generated Merkle trees
// verify successfully.
TEST_P(CTLogVerifierTestUsingGenerator, VerifiesValidConsistencyProof) {
  const size_t tree_size = GetParam();

  std::vector<std::string> tree_leaves(tree_size);
  for (size_t i = 0; i < tree_size; ++i)
    tree_leaves[i].push_back(static_cast<char>(i));

  const std::string tree_root =
      rfc6962::HashTree(tree_leaves.data(), tree_size);

  // Check consistency proofs for every sub-tree.
  for (size_t old_tree_size = 0; old_tree_size <= tree_size; ++old_tree_size) {
    SCOPED_TRACE(old_tree_size);
    const std::string old_tree_root =
        rfc6962::HashTree(tree_leaves.data(), old_tree_size);
    const std::vector<std::string> proof = rfc6962::CreateConsistencyProof(
        tree_leaves.data(), tree_size, old_tree_size);
    // Checks that the consistency proof verifies only with the correct tree
    // sizes and root hashes.
    CheckVerifyConsistencyProof(*log_, old_tree_size, tree_size, old_tree_root,
                                tree_root, proof);
  }
}

// Checks that valid audit proofs for a range of generated Merkle trees verify
// successfully.
TEST_P(CTLogVerifierTestUsingGenerator, VerifiesValidAuditProofs) {
  const size_t tree_size = GetParam();

  std::vector<std::string> tree_leaves(tree_size);
  for (size_t i = 0; i < tree_size; ++i)
    tree_leaves[i].push_back(static_cast<char>(i));

  const std::string root = rfc6962::HashTree(tree_leaves.data(), tree_size);

  // Check audit proofs for every leaf in the tree.
  for (size_t leaf = 0; leaf < tree_size; ++leaf) {
    SCOPED_TRACE(leaf);
    std::vector<std::string> proof =
        rfc6962::CreateAuditProof(tree_leaves.data(), tree_size, leaf);
    // Checks that the audit proof verifies only for this leaf data, index,
    // hash, tree size and root hash.
    CheckVerifyAuditProof(*log_, leaf, tree_size, proof, root,
                          rfc6962::HashLeaf(tree_leaves[leaf]));
  }
}

// Test verification of consistency proofs and audit proofs for all tree sizes
// from 0 to 128.
INSTANTIATE_TEST_SUITE_P(RangeOfTreeSizes,
                         CTLogVerifierTestUsingGenerator,
                         testing::Range(size_t(0), size_t(129)));

}  // namespace

}  // namespace net
