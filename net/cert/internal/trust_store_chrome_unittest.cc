// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_chrome.h"

#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "crypto/sha2.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"

namespace net {
namespace {

#include "net/data/ssl/chrome_root_store/chrome-root-store-test-data-inc.cc"

std::shared_ptr<const bssl::ParsedCertificate> ToParsedCertificate(
    const X509Certificate& cert) {
  bssl::CertErrors errors;
  std::shared_ptr<const bssl::ParsedCertificate> parsed =
      bssl::ParsedCertificate::Create(
          bssl::UpRef(cert.cert_buffer()),
          x509_util::DefaultParseCertificateOptions(), &errors);
  EXPECT_TRUE(parsed) << errors.ToDebugString();
  return parsed;
}

scoped_refptr<X509Certificate> MakeTestRoot() {
  auto builder = std::make_unique<CertBuilder>(nullptr, nullptr);
  auto now = base::Time::Now();
  builder->SetValidity(now - base::Days(1), now + base::Days(1));
  builder->SetBasicConstraints(/*is_ca=*/true, /*path_len=*/-1);
  builder->SetKeyUsages(
      {bssl::KEY_USAGE_BIT_KEY_CERT_SIGN, bssl::KEY_USAGE_BIT_CRL_SIGN});
  return builder->GetX509Certificate();
}

TEST(TrustStoreChromeTestNoFixture, ContainsCert) {
  std::unique_ptr<TrustStoreChrome> trust_store_chrome =
      TrustStoreChrome::CreateTrustStoreForTesting(
          base::span<const ChromeRootCertInfo>(kChromeRootCertList),
          /*version=*/1);

  // Check every certificate in test_store.certs is included.
  CertificateList certs = CreateCertificateListFromFile(
      GetTestNetDataDirectory().AppendASCII("ssl/chrome_root_store"),
      "test_store.certs", X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_EQ(certs.size(), 2u);

  for (const auto& cert : certs) {
    std::shared_ptr<const bssl::ParsedCertificate> parsed =
        ToParsedCertificate(*cert);
    ASSERT_TRUE(trust_store_chrome->Contains(parsed.get()));
    bssl::CertificateTrust trust = trust_store_chrome->GetTrust(parsed.get());
    EXPECT_EQ(bssl::CertificateTrust::ForTrustAnchor().ToDebugString(),
              trust.ToDebugString());
  }

  // Other certificates should not be included. Which test cert used here isn't
  // important as long as it isn't one of the certificates in the
  // chrome_root_store/test_store.certs.
  scoped_refptr<X509Certificate> other_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(other_cert);
  std::shared_ptr<const bssl::ParsedCertificate> other_parsed =
      ToParsedCertificate(*other_cert);
  ASSERT_FALSE(trust_store_chrome->Contains(other_parsed.get()));
  bssl::CertificateTrust trust =
      trust_store_chrome->GetTrust(other_parsed.get());
  EXPECT_EQ(bssl::CertificateTrust::ForUnspecified().ToDebugString(),
            trust.ToDebugString());
}

TEST(TrustStoreChromeTestNoFixture, Constraints) {
  std::unique_ptr<TrustStoreChrome> trust_store_chrome =
      TrustStoreChrome::CreateTrustStoreForTesting(
          base::span<const ChromeRootCertInfo>(kChromeRootCertList),
          /*version=*/1);

  const std::string kUnconstrainedCertHash =
      "568d6905a2c88708a4b3025190edcfedb1974a606a13c6e5290fcb2ae63edab5";
  const std::string kConstrainedCertHash =
      "6b9c08e86eb0f767cfad65cd98b62149e5494a67f5845e7bd1ed019f27b86bd6";

  std::shared_ptr<const bssl::ParsedCertificate> constrained_cert;
  std::shared_ptr<const bssl::ParsedCertificate> unconstrained_cert;

  CertificateList certs = CreateCertificateListFromFile(
      GetTestNetDataDirectory().AppendASCII("ssl/chrome_root_store"),
      "test_store.certs", X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_EQ(certs.size(), 2u);
  for (const auto& cert : certs) {
    std::shared_ptr<const bssl::ParsedCertificate> parsed =
        ToParsedCertificate(*cert);
    std::string sha256_hex = base::ToLowerASCII(
        base::HexEncode(crypto::SHA256Hash(parsed->der_cert())));
    if (sha256_hex == kConstrainedCertHash) {
      constrained_cert = parsed;
    } else if (sha256_hex == kUnconstrainedCertHash) {
      unconstrained_cert = parsed;
    }
  }

  ASSERT_TRUE(unconstrained_cert);
  EXPECT_TRUE(
      trust_store_chrome->GetConstraintsForCert(unconstrained_cert.get())
          .empty());

  ASSERT_TRUE(constrained_cert);
  base::span<const ChromeRootCertConstraints> constraints =
      trust_store_chrome->GetConstraintsForCert(constrained_cert.get());
  ASSERT_EQ(constraints.size(), 2U);

  EXPECT_FALSE(constraints[0].sct_all_after.has_value());
  ASSERT_TRUE(constraints[0].sct_not_after.has_value());
  EXPECT_EQ(
      constraints[0].sct_not_after.value().InMillisecondsSinceUnixEpoch() /
          1000,
      0x5af);
  EXPECT_FALSE(constraints[0].min_version.has_value());
  ASSERT_TRUE(constraints[0].max_version_exclusive.has_value());
  EXPECT_EQ(constraints[0].max_version_exclusive.value().components(),
            std::vector<uint32_t>({125, 0, 6368, 2}));

  EXPECT_FALSE(constraints[1].sct_not_after.has_value());
  ASSERT_TRUE(constraints[1].sct_all_after.has_value());
  EXPECT_EQ(
      constraints[1].sct_all_after.value().InMillisecondsSinceUnixEpoch() /
          1000,
      0x2579);
  ASSERT_TRUE(constraints[1].min_version.has_value());
  EXPECT_FALSE(constraints[1].max_version_exclusive.has_value());
  EXPECT_EQ(constraints[1].min_version.value().components(),
            std::vector<uint32_t>({128}));

  // Other certificates should return nullptr if they are queried for CRS
  // constraints. Which test cert used here isn't important as long as it isn't
  // one of the certificates in the chrome_root_store/test_store.certs.
  scoped_refptr<X509Certificate> other_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(other_cert);
  std::shared_ptr<const bssl::ParsedCertificate> other_parsed =
      ToParsedCertificate(*other_cert);
  ASSERT_TRUE(other_parsed);
  EXPECT_FALSE(trust_store_chrome->Contains(other_parsed.get()));
  EXPECT_TRUE(
      trust_store_chrome->GetConstraintsForCert(other_parsed.get()).empty());
}

TEST(TrustStoreChromeTestNoFixture, OverrideConstraints) {
  // Root1: has no constraints and no override constraints
  // Root2: has constraints and no override constraints
  // Root3: has no constraints and has override constraints
  // Root4: has constraints and has override constraints
  // Root5: not present in CRS and no override constraints
  // Root6: not present in CRS but has override constraints
  scoped_refptr<X509Certificate> root1 = MakeTestRoot();
  scoped_refptr<X509Certificate> root2 = MakeTestRoot();
  scoped_refptr<X509Certificate> root3 = MakeTestRoot();
  scoped_refptr<X509Certificate> root4 = MakeTestRoot();
  scoped_refptr<X509Certificate> root5 = MakeTestRoot();
  scoped_refptr<X509Certificate> root6 = MakeTestRoot();

  std::vector<StaticChromeRootCertConstraints> c2 = {{.min_version = "20"}};
  std::vector<StaticChromeRootCertConstraints> c4 = {{.min_version = "40"}};
  std::vector<ChromeRootCertInfo> root_cert_info = {
      {root1->cert_span(), {}},
      {root2->cert_span(), c2},
      {root3->cert_span(), {}},
      {root4->cert_span(), c4},
  };

  base::flat_map<std::array<uint8_t, crypto::kSHA256Length>,
                 std::vector<ChromeRootCertConstraints>>
      override_constraints;

  override_constraints[crypto::SHA256Hash(root3->cert_span())] = {
      {std::nullopt, std::nullopt, std::nullopt,
       /*max_version_exclusive=*/std::make_optional(base::Version("31"))}};

  override_constraints[crypto::SHA256Hash(root4->cert_span())] = {
      {std::nullopt, std::nullopt, std::nullopt,
       /*max_version_exclusive=*/std::make_optional(base::Version("41"))}};

  override_constraints[crypto::SHA256Hash(root6->cert_span())] = {
      {std::nullopt, std::nullopt, std::nullopt,
       /*max_version_exclusive=*/std::make_optional(base::Version("61"))}};

  std::unique_ptr<TrustStoreChrome> trust_store_chrome =
      TrustStoreChrome::CreateTrustStoreForTesting(
          std::move(root_cert_info),
          /*version=*/1, std::move(override_constraints));

  {
    std::shared_ptr<const bssl::ParsedCertificate> parsed =
        ToParsedCertificate(*root1);
    ASSERT_TRUE(parsed);
    EXPECT_TRUE(trust_store_chrome->Contains(parsed.get()));
    EXPECT_TRUE(
        trust_store_chrome->GetConstraintsForCert(parsed.get()).empty());
  }

  {
    std::shared_ptr<const bssl::ParsedCertificate> parsed =
        ToParsedCertificate(*root2);
    ASSERT_TRUE(parsed);
    EXPECT_TRUE(trust_store_chrome->Contains(parsed.get()));

    base::span<const ChromeRootCertConstraints> constraints =
        trust_store_chrome->GetConstraintsForCert(parsed.get());
    ASSERT_EQ(constraints.size(), 1U);
    EXPECT_EQ(constraints[0].min_version.value().components(),
              std::vector<uint32_t>({20}));
    EXPECT_FALSE(constraints[0].max_version_exclusive.has_value());
  }

  {
    std::shared_ptr<const bssl::ParsedCertificate> parsed =
        ToParsedCertificate(*root3);
    ASSERT_TRUE(parsed);
    EXPECT_TRUE(trust_store_chrome->Contains(parsed.get()));

    base::span<const ChromeRootCertConstraints> constraints =
        trust_store_chrome->GetConstraintsForCert(parsed.get());
    ASSERT_EQ(constraints.size(), 1U);
    EXPECT_FALSE(constraints[0].min_version.has_value());
    EXPECT_EQ(constraints[0].max_version_exclusive.value().components(),
              std::vector<uint32_t>({31}));
  }

  {
    std::shared_ptr<const bssl::ParsedCertificate> parsed =
        ToParsedCertificate(*root4);
    ASSERT_TRUE(parsed);
    EXPECT_TRUE(trust_store_chrome->Contains(parsed.get()));

    base::span<const ChromeRootCertConstraints> constraints =
        trust_store_chrome->GetConstraintsForCert(parsed.get());
    ASSERT_EQ(constraints.size(), 1U);
    EXPECT_FALSE(constraints[0].min_version.has_value());
    EXPECT_EQ(constraints[0].max_version_exclusive.value().components(),
              std::vector<uint32_t>({41}));
  }

  {
    std::shared_ptr<const bssl::ParsedCertificate> parsed =
        ToParsedCertificate(*root5);
    ASSERT_TRUE(parsed);
    EXPECT_FALSE(trust_store_chrome->Contains(parsed.get()));
    EXPECT_TRUE(
        trust_store_chrome->GetConstraintsForCert(parsed.get()).empty());
  }

  {
    std::shared_ptr<const bssl::ParsedCertificate> parsed =
        ToParsedCertificate(*root6);
    ASSERT_TRUE(parsed);
    EXPECT_FALSE(trust_store_chrome->Contains(parsed.get()));

    base::span<const ChromeRootCertConstraints> constraints =
        trust_store_chrome->GetConstraintsForCert(parsed.get());
    ASSERT_EQ(constraints.size(), 1U);
    EXPECT_FALSE(constraints[0].min_version.has_value());
    EXPECT_EQ(constraints[0].max_version_exclusive.value().components(),
              std::vector<uint32_t>({61}));
  }
}

TEST(TrustStoreChromeTestNoFixture, ParseCommandLineConstraintsEmpty) {
  EXPECT_TRUE(TrustStoreChrome::ParseCrsConstraintsSwitch("").empty());
  EXPECT_TRUE(TrustStoreChrome::ParseCrsConstraintsSwitch("invalid").empty());
  EXPECT_TRUE(TrustStoreChrome::ParseCrsConstraintsSwitch(
                  "invalidhash:sctnotafter=123456")
                  .empty());
}

TEST(TrustStoreChromeTestNoFixture, ParseCommandLineConstraintsErrorHandling) {
  auto constraints = TrustStoreChrome::ParseCrsConstraintsSwitch(
      // Valid hash and valid constraint name with invalid value (missing `,`
      // between constraints, so sctallafter value will not be parsable as an
      // integer). Should result in a constraintset with every constraint
      // being nullopt.
      "568c8ef6b526d1394bca052ba3e4d1f4d7a8d9c88c55a1a9ab7ca0fae2dc5473:"
      "sctallafter=9876543sctnotafter=1234567890+"
      // Invalid hash (valid hex, but too short).
      "37a9761b69457987abbc8636182d8273498719659716397401f98e019b20a9:"
      "sctallafter=9876543+"
      // Invalid hash (valid hex, but too long).
      "37a9761b69457987abbc8636182d8273498719659716397401f98e019b20a91111:"
      "sctallafter=9876543+"
      // Invalid constraint mapping (missing `:` between hash and constraint).
      "737a9761b69457987abbc8636182d8273498719659716397401f98e019b20a98"
      "sctallafter=9876543+"
      // Invalid and valid hashes with both invalid and valid constraints.
      "11,a7e0c75d7f772fccf26a6ac1f7b0a86a482e2f3d326bc911c95d56ff3d4906d5,22:"
      "invalidconstraint=hello,sctnotafter=789012345+"
      // Missing `+` between constraint mappings.
      // This will parse the next hash and minversion all as an invalid
      // sctallafter value and then the maxversionexclusive will apply to the
      // previous root hash.
      "65ee41e8a8c27b71b6bfcf44653c8e8370ec5e106e272592c2fbcbadf8dc5763:"
      "sctnotafter=123456,sctallafter=54321"
      "3333333333333333333333333333333333333333333333333333333333333333:"
      "minversion=1,maxversionexclusive=2.3");
  EXPECT_EQ(constraints.size(), 3U);

  {
    constexpr uint8_t hash[] = {0x56, 0x8c, 0x8e, 0xf6, 0xb5, 0x26, 0xd1, 0x39,
                                0x4b, 0xca, 0x05, 0x2b, 0xa3, 0xe4, 0xd1, 0xf4,
                                0xd7, 0xa8, 0xd9, 0xc8, 0x8c, 0x55, 0xa1, 0xa9,
                                0xab, 0x7c, 0xa0, 0xfa, 0xe2, 0xdc, 0x54, 0x73};
    auto it = constraints.find(base::make_span(hash));
    ASSERT_NE(it, constraints.end());
    ASSERT_EQ(it->second.size(), 1U);
    const auto& constraint1 = it->second[0];
    EXPECT_FALSE(constraint1.sct_not_after.has_value());
    EXPECT_FALSE(constraint1.sct_all_after.has_value());
    EXPECT_FALSE(constraint1.min_version.has_value());
    EXPECT_FALSE(constraint1.max_version_exclusive.has_value());
  }
  {
    constexpr uint8_t hash[] = {0xa7, 0xe0, 0xc7, 0x5d, 0x7f, 0x77, 0x2f, 0xcc,
                                0xf2, 0x6a, 0x6a, 0xc1, 0xf7, 0xb0, 0xa8, 0x6a,
                                0x48, 0x2e, 0x2f, 0x3d, 0x32, 0x6b, 0xc9, 0x11,
                                0xc9, 0x5d, 0x56, 0xff, 0x3d, 0x49, 0x06, 0xd5};
    auto it = constraints.find(base::make_span(hash));
    ASSERT_NE(it, constraints.end());
    ASSERT_EQ(it->second.size(), 1U);

    const auto& constraint1 = it->second[0];
    ASSERT_TRUE(constraint1.sct_not_after.has_value());
    EXPECT_EQ(constraint1.sct_not_after->InMillisecondsSinceUnixEpoch() / 1000,
              789012345);
    EXPECT_FALSE(constraint1.sct_all_after.has_value());
    EXPECT_FALSE(constraint1.min_version.has_value());
    EXPECT_FALSE(constraint1.max_version_exclusive.has_value());
  }

  {
    unsigned char hash[] = {0x65, 0xee, 0x41, 0xe8, 0xa8, 0xc2, 0x7b, 0x71,
                            0xb6, 0xbf, 0xcf, 0x44, 0x65, 0x3c, 0x8e, 0x83,
                            0x70, 0xec, 0x5e, 0x10, 0x6e, 0x27, 0x25, 0x92,
                            0xc2, 0xfb, 0xcb, 0xad, 0xf8, 0xdc, 0x57, 0x63};

    auto it = constraints.find(base::make_span(hash));
    ASSERT_NE(it, constraints.end());
    ASSERT_EQ(it->second.size(), 1U);
    const auto& constraint = it->second[0];
    ASSERT_TRUE(constraint.sct_not_after.has_value());
    EXPECT_EQ(constraint.sct_not_after->InMillisecondsSinceUnixEpoch() / 1000,
              123456);
    EXPECT_FALSE(constraint.sct_all_after.has_value());
    EXPECT_FALSE(constraint.min_version.has_value());
    EXPECT_EQ(constraint.max_version_exclusive, base::Version({2, 3}));
  }
}

TEST(TrustStoreChromeTestNoFixture,
     ParseCommandLineConstraintsOneRootOneConstraint) {
  auto constraints = TrustStoreChrome::ParseCrsConstraintsSwitch(
      "65ee41e8a8c27b71b6bfcf44653c8e8370ec5e106e272592c2fbcbadf8dc5763:"
      "sctnotafter=123456");
  EXPECT_EQ(constraints.size(), 1U);
  unsigned char hash[] = {0x65, 0xee, 0x41, 0xe8, 0xa8, 0xc2, 0x7b, 0x71,
                          0xb6, 0xbf, 0xcf, 0x44, 0x65, 0x3c, 0x8e, 0x83,
                          0x70, 0xec, 0x5e, 0x10, 0x6e, 0x27, 0x25, 0x92,
                          0xc2, 0xfb, 0xcb, 0xad, 0xf8, 0xdc, 0x57, 0x63};

  auto it = constraints.find(base::make_span(hash));
  ASSERT_NE(it, constraints.end());
  ASSERT_EQ(it->second.size(), 1U);
  const auto& constraint = it->second[0];
  ASSERT_TRUE(constraint.sct_not_after.has_value());
  EXPECT_EQ(constraint.sct_not_after->InMillisecondsSinceUnixEpoch() / 1000,
            123456);
  EXPECT_FALSE(constraint.sct_all_after.has_value());
  EXPECT_FALSE(constraint.min_version.has_value());
  EXPECT_FALSE(constraint.max_version_exclusive.has_value());
}

TEST(TrustStoreChromeTestNoFixture,
     ParseCommandLineConstraintsMultipleRootsMultipleConstraints) {
  auto constraints = TrustStoreChrome::ParseCrsConstraintsSwitch(
      "784ecaa8b9dfcc826547f806f759abd6b4481582fc7e377dc3e6a0a959025126,"
      "a7e0c75d7f772fccf26a6ac1f7b0a86a482e2f3d326bc911c95d56ff3d4906d5:"
      "sctnotafter=123456,sctallafter=7689,"
      "minversion=1.2.3.4,maxversionexclusive=10+"
      "a7e0c75d7f772fccf26a6ac1f7b0a86a482e2f3d326bc911c95d56ff3d4906d5,"
      "568c8ef6b526d1394bca052ba3e4d1f4d7a8d9c88c55a1a9ab7ca0fae2dc5473:"
      "sctallafter=9876543,sctnotafter=1234567890");
  EXPECT_EQ(constraints.size(), 3U);

  {
    constexpr uint8_t hash1[] = {
        0x78, 0x4e, 0xca, 0xa8, 0xb9, 0xdf, 0xcc, 0x82, 0x65, 0x47, 0xf8,
        0x06, 0xf7, 0x59, 0xab, 0xd6, 0xb4, 0x48, 0x15, 0x82, 0xfc, 0x7e,
        0x37, 0x7d, 0xc3, 0xe6, 0xa0, 0xa9, 0x59, 0x02, 0x51, 0x26};
    auto it = constraints.find(base::make_span(hash1));
    ASSERT_NE(it, constraints.end());
    ASSERT_EQ(it->second.size(), 1U);
    const auto& constraint1 = it->second[0];
    ASSERT_TRUE(constraint1.sct_not_after.has_value());
    EXPECT_EQ(constraint1.sct_not_after->InMillisecondsSinceUnixEpoch() / 1000,
              123456);
    ASSERT_TRUE(constraint1.sct_all_after.has_value());
    EXPECT_EQ(constraint1.sct_all_after->InMillisecondsSinceUnixEpoch() / 1000,
              7689);
    EXPECT_EQ(constraint1.min_version, base::Version({1, 2, 3, 4}));
    EXPECT_EQ(constraint1.max_version_exclusive, base::Version({10}));
  }

  {
    constexpr uint8_t hash2[] = {
        0xa7, 0xe0, 0xc7, 0x5d, 0x7f, 0x77, 0x2f, 0xcc, 0xf2, 0x6a, 0x6a,
        0xc1, 0xf7, 0xb0, 0xa8, 0x6a, 0x48, 0x2e, 0x2f, 0x3d, 0x32, 0x6b,
        0xc9, 0x11, 0xc9, 0x5d, 0x56, 0xff, 0x3d, 0x49, 0x06, 0xd5};
    auto it = constraints.find(base::make_span(hash2));
    ASSERT_NE(it, constraints.end());
    ASSERT_EQ(it->second.size(), 2U);

    const auto& constraint1 = it->second[0];
    ASSERT_TRUE(constraint1.sct_not_after.has_value());
    EXPECT_EQ(constraint1.sct_not_after->InMillisecondsSinceUnixEpoch() / 1000,
              123456);
    ASSERT_TRUE(constraint1.sct_all_after.has_value());
    EXPECT_EQ(constraint1.sct_all_after->InMillisecondsSinceUnixEpoch() / 1000,
              7689);
    EXPECT_EQ(constraint1.min_version, base::Version({1, 2, 3, 4}));
    EXPECT_EQ(constraint1.max_version_exclusive, base::Version({10}));

    const auto& constraint2 = it->second[1];
    ASSERT_TRUE(constraint2.sct_not_after.has_value());
    EXPECT_EQ(constraint2.sct_not_after->InMillisecondsSinceUnixEpoch() / 1000,
              1234567890);
    ASSERT_TRUE(constraint2.sct_all_after.has_value());
    EXPECT_EQ(constraint2.sct_all_after->InMillisecondsSinceUnixEpoch() / 1000,
              9876543);
    EXPECT_FALSE(constraint2.min_version.has_value());
    EXPECT_FALSE(constraint2.max_version_exclusive.has_value());
  }

  {
    constexpr uint8_t hash3[] = {
        0x56, 0x8c, 0x8e, 0xf6, 0xb5, 0x26, 0xd1, 0x39, 0x4b, 0xca, 0x05,
        0x2b, 0xa3, 0xe4, 0xd1, 0xf4, 0xd7, 0xa8, 0xd9, 0xc8, 0x8c, 0x55,
        0xa1, 0xa9, 0xab, 0x7c, 0xa0, 0xfa, 0xe2, 0xdc, 0x54, 0x73};
    auto it = constraints.find(base::make_span(hash3));
    ASSERT_NE(it, constraints.end());
    ASSERT_EQ(it->second.size(), 1U);
    const auto& constraint1 = it->second[0];
    ASSERT_TRUE(constraint1.sct_not_after.has_value());
    EXPECT_EQ(constraint1.sct_not_after->InMillisecondsSinceUnixEpoch() / 1000,
              1234567890);
    ASSERT_TRUE(constraint1.sct_all_after.has_value());
    EXPECT_EQ(constraint1.sct_all_after->InMillisecondsSinceUnixEpoch() / 1000,
              9876543);
    EXPECT_FALSE(constraint1.min_version.has_value());
    EXPECT_FALSE(constraint1.max_version_exclusive.has_value());
  }
}

}  // namespace
}  // namespace net
