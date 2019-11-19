// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_collection.h"

#include "net/cert/internal/test_helpers.h"
#include "net/cert/internal/trust_store_in_memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class TrustStoreCollectionTest : public testing::Test {
 public:
  void SetUp() override {
    ParsedCertificateList chain;
    ASSERT_TRUE(ReadCertChainFromFile(
        "net/data/verify_certificate_chain_unittest/key-rollover/oldchain.pem",
        &chain));

    ASSERT_EQ(3U, chain.size());
    target_ = chain[0];
    oldintermediate_ = chain[1];
    oldroot_ = chain[2];
    ASSERT_TRUE(target_);
    ASSERT_TRUE(oldintermediate_);
    ASSERT_TRUE(oldroot_);

    ASSERT_TRUE(
        ReadCertChainFromFile("net/data/verify_certificate_chain_unittest/"
                              "key-rollover/longrolloverchain.pem",
                              &chain));

    ASSERT_EQ(5U, chain.size());
    newintermediate_ = chain[1];
    newroot_ = chain[2];
    newrootrollover_ = chain[3];
    ASSERT_TRUE(newintermediate_);
    ASSERT_TRUE(newroot_);
    ASSERT_TRUE(newrootrollover_);
  }

 protected:
  scoped_refptr<ParsedCertificate> oldroot_;
  scoped_refptr<ParsedCertificate> newroot_;
  scoped_refptr<ParsedCertificate> newrootrollover_;

  scoped_refptr<ParsedCertificate> target_;
  scoped_refptr<ParsedCertificate> oldintermediate_;
  scoped_refptr<ParsedCertificate> newintermediate_;
};

// Collection contains no stores, should return no results.
TEST_F(TrustStoreCollectionTest, NoStores) {
  ParsedCertificateList issuers;

  TrustStoreCollection collection;
  collection.SyncGetIssuersOf(target_.get(), &issuers);

  EXPECT_TRUE(issuers.empty());
}

// Collection contains only one store.
TEST_F(TrustStoreCollectionTest, OneStore) {
  ParsedCertificateList issuers;

  TrustStoreCollection collection;
  TrustStoreInMemory in_memory;
  in_memory.AddTrustAnchor(newroot_);
  collection.AddTrustStore(&in_memory);
  collection.SyncGetIssuersOf(newintermediate_.get(), &issuers);

  ASSERT_EQ(1U, issuers.size());
  EXPECT_EQ(newroot_.get(), issuers[0].get());

  // newroot_ is trusted.
  CertificateTrust trust;
  collection.GetTrust(newroot_, &trust, /*debug_data=*/nullptr);
  EXPECT_EQ(CertificateTrustType::TRUSTED_ANCHOR, trust.type);

  // oldroot_ is not.
  collection.GetTrust(oldroot_, &trust, /*debug_data=*/nullptr);
  EXPECT_EQ(CertificateTrustType::UNSPECIFIED, trust.type);
}

// SyncGetIssuersOf() should append to its output parameters rather than assign
// them.
TEST_F(TrustStoreCollectionTest, OutputVectorsAppendedTo) {
  ParsedCertificateList issuers;

  // Populate the out-parameter with some values.
  issuers.resize(3);

  TrustStoreCollection collection;
  TrustStoreInMemory in_memory;
  in_memory.AddTrustAnchor(newroot_);
  collection.AddTrustStore(&in_memory);
  collection.SyncGetIssuersOf(newintermediate_.get(), &issuers);

  ASSERT_EQ(4U, issuers.size());
  EXPECT_EQ(newroot_.get(), issuers[3].get());

  // newroot_ is trusted.
  CertificateTrust trust;
  collection.GetTrust(newroot_, &trust, /*debug_data=*/nullptr);
  EXPECT_EQ(CertificateTrustType::TRUSTED_ANCHOR, trust.type);

  // newrootrollover_ is not.
  collection.GetTrust(newrootrollover_, &trust, /*debug_data=*/nullptr);
  EXPECT_EQ(CertificateTrustType::UNSPECIFIED, trust.type);
}

// Collection contains two stores.
TEST_F(TrustStoreCollectionTest, TwoStores) {
  ParsedCertificateList issuers;

  TrustStoreCollection collection;
  TrustStoreInMemory in_memory1;
  TrustStoreInMemory in_memory2;
  in_memory1.AddTrustAnchor(newroot_);
  in_memory2.AddTrustAnchor(oldroot_);
  collection.AddTrustStore(&in_memory1);
  collection.AddTrustStore(&in_memory2);
  collection.SyncGetIssuersOf(newintermediate_.get(), &issuers);

  ASSERT_EQ(2U, issuers.size());
  EXPECT_EQ(newroot_.get(), issuers[0].get());
  EXPECT_EQ(oldroot_.get(), issuers[1].get());

  // newroot_ is trusted.
  CertificateTrust trust;
  collection.GetTrust(newroot_, &trust, /*debug_data=*/nullptr);
  EXPECT_EQ(CertificateTrustType::TRUSTED_ANCHOR, trust.type);

  // oldroot_ is trusted.
  collection.GetTrust(oldroot_, &trust, /*debug_data=*/nullptr);
  EXPECT_EQ(CertificateTrustType::TRUSTED_ANCHOR, trust.type);

  // newrootrollover_ is not.
  collection.GetTrust(newrootrollover_, &trust, /*debug_data=*/nullptr);
  EXPECT_EQ(CertificateTrustType::UNSPECIFIED, trust.type);
}

// Collection contains two stores. The certificate is marked as trusted in one,
// but distrusted in the other.
TEST_F(TrustStoreCollectionTest, DistrustTakesPriority) {
  ParsedCertificateList issuers;

  TrustStoreCollection collection;
  TrustStoreInMemory in_memory1;
  TrustStoreInMemory in_memory2;

  // newroot_ is trusted in store1, distrusted in store2.
  in_memory1.AddTrustAnchor(newroot_);
  in_memory2.AddDistrustedCertificateForTest(newroot_);

  // oldintermediate is distrusted in store1, trusted in store2.
  in_memory1.AddDistrustedCertificateForTest(oldintermediate_);
  in_memory2.AddTrustAnchor(oldintermediate_);

  collection.AddTrustStore(&in_memory1);
  collection.AddTrustStore(&in_memory2);

  // newroot_ is distrusted..
  CertificateTrust trust;
  collection.GetTrust(newroot_, &trust, /*debug_data=*/nullptr);
  EXPECT_EQ(CertificateTrustType::DISTRUSTED, trust.type);

  // oldintermediate_ is distrusted.
  collection.GetTrust(oldintermediate_, &trust, /*debug_data=*/nullptr);
  EXPECT_EQ(CertificateTrustType::DISTRUSTED, trust.type);

  // newrootrollover_ is unspecified.
  collection.GetTrust(newrootrollover_, &trust, /*debug_data=*/nullptr);
  EXPECT_EQ(CertificateTrustType::UNSPECIFIED, trust.type);
}

}  // namespace

}  // namespace net
