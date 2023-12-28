// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/merkle_tree_leaf.h"

#include <string.h>

#include <string>

#include "base/strings/string_number_conversions.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/ct_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::ct {

namespace {

MATCHER_P(HexEq, hexStr, "") {
  std::vector<uint8_t> bytes;

  if (!base::HexStringToBytes(hexStr, &bytes)) {
    *result_listener << "expected value was not a valid hex string";
    return false;
  }

  if (bytes.size() != arg.size()) {
    *result_listener << "expected and actual are different lengths";
    return false;
  }

  // Make sure we don't pass nullptrs to memcmp
  if (arg.empty())
    return true;

  // Print hex string (easier to read than default GTest representation)
  *result_listener << "a.k.a. 0x" << base::HexEncode(arg.data(), arg.size());
  return memcmp(arg.data(), bytes.data(), bytes.size()) == 0;
}

class MerkleTreeLeafTest : public ::testing::Test {
 public:
  void SetUp() override {
    std::string der_test_cert(ct::GetDerEncodedX509Cert());
    test_cert_ =
        X509Certificate::CreateFromBytes(base::as_byte_span(der_test_cert));
    ASSERT_TRUE(test_cert_);

    GetX509CertSCT(&x509_sct_);
    x509_sct_->origin = SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE;

    test_precert_ = CreateCertificateChainFromFile(
        GetTestCertsDirectory(), "ct-test-embedded-cert.pem",
        X509Certificate::FORMAT_AUTO);
    ASSERT_TRUE(test_precert_);
    ASSERT_EQ(1u, test_precert_->intermediate_buffers().size());
    GetPrecertSCT(&precert_sct_);
    precert_sct_->origin = SignedCertificateTimestamp::SCT_EMBEDDED;
  }

 protected:
  scoped_refptr<SignedCertificateTimestamp> x509_sct_;
  scoped_refptr<SignedCertificateTimestamp> precert_sct_;
  scoped_refptr<X509Certificate> test_cert_;
  scoped_refptr<X509Certificate> test_precert_;
};

TEST_F(MerkleTreeLeafTest, CreatesForX509Cert) {
  MerkleTreeLeaf leaf;
  ASSERT_TRUE(GetMerkleTreeLeaf(test_cert_.get(), x509_sct_.get(), &leaf));

  EXPECT_EQ(SignedEntryData::LOG_ENTRY_TYPE_X509, leaf.signed_entry.type);
  EXPECT_FALSE(leaf.signed_entry.leaf_certificate.empty());
  EXPECT_TRUE(leaf.signed_entry.tbs_certificate.empty());

  EXPECT_EQ(x509_sct_->timestamp, leaf.timestamp);
  EXPECT_EQ(x509_sct_->extensions, leaf.extensions);
}

TEST_F(MerkleTreeLeafTest, CreatesForPrecert) {
  MerkleTreeLeaf leaf;
  ASSERT_TRUE(
      GetMerkleTreeLeaf(test_precert_.get(), precert_sct_.get(), &leaf));

  EXPECT_EQ(SignedEntryData::LOG_ENTRY_TYPE_PRECERT, leaf.signed_entry.type);
  EXPECT_FALSE(leaf.signed_entry.tbs_certificate.empty());
  EXPECT_TRUE(leaf.signed_entry.leaf_certificate.empty());

  EXPECT_EQ(precert_sct_->timestamp, leaf.timestamp);
  EXPECT_EQ(precert_sct_->extensions, leaf.extensions);
}

TEST_F(MerkleTreeLeafTest, DoesNotCreateForEmbeddedSCTButNotPrecert) {
  MerkleTreeLeaf leaf;
  ASSERT_FALSE(GetMerkleTreeLeaf(test_cert_.get(), precert_sct_.get(), &leaf));
}

// Expected hashes calculated by:
// 1. Writing the serialized tree leaves from
//    CtSerialization::EncodesLogEntryFor{X509Cert,Precert} to files.
// 2. Prepending a zero byte to both files.
// 3. Passing each file through the sha256sum tool.

TEST_F(MerkleTreeLeafTest, HashForX509Cert) {
  MerkleTreeLeaf leaf;
  ct::GetX509CertTreeLeaf(&leaf);

  std::string hash;
  ASSERT_TRUE(HashMerkleTreeLeaf(leaf, &hash));
  EXPECT_THAT(hash, HexEq("452da788b3b8d15872ff0bb0777354b2a7f1c1887b5633201e76"
                          "2ba5a4b143fc"));
}

TEST_F(MerkleTreeLeafTest, HashForPrecert) {
  MerkleTreeLeaf leaf;
  ct::GetPrecertTreeLeaf(&leaf);

  std::string hash;
  ASSERT_TRUE(HashMerkleTreeLeaf(leaf, &hash));
  EXPECT_THAT(hash, HexEq("257ae85f08810445511e35e33f7aee99ee19407971e35e95822b"
                          "bf42a74be223"));
}

}  // namespace

}  // namespace net::ct
