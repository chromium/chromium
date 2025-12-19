// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_chrome.h"

#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "crypto/sha2.h"
#include "net/cert/root_store_proto_lite/root_store.pb.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
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

std::shared_ptr<const bssl::ParsedCertificate>
FindParsedCertificateInCertificateList(const std::string& hash,
                                       CertificateList certs) {
  for (const auto& cert : certs) {
    std::shared_ptr<const bssl::ParsedCertificate> parsed =
        ToParsedCertificate(*cert);
    std::string sha256_hex =
        base::HexEncodeLower(crypto::SHA256Hash(parsed->der_cert()));
    if (sha256_hex == hash) {
      return parsed;
    }
  }
  return nullptr;
}

TEST(TrustStoreChromeTestNoFixture, ContainsCert) {
  std::unique_ptr<TrustStoreChrome> trust_store_chrome =
      TrustStoreChrome::CreateTrustStoreForTesting(
          base::span<const ChromeRootCertInfo>(kChromeRootCertList),
          base::span(kEutlRootCertList),
          /*version=*/1);

  // Check every certificate in test_store.certs is included.
  CertificateList certs = CreateCertificateListFromFile(
      GetTestNetDataDirectory().AppendASCII("ssl/chrome_root_store"),
      "test_store.certs", X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_EQ(certs.size(), 6u);

  size_t eutl_certs = 0;
  for (const auto& cert : certs) {
    std::shared_ptr<const bssl::ParsedCertificate> parsed =
        ToParsedCertificate(*cert);
    ASSERT_TRUE(trust_store_chrome->Contains(parsed.get()));
    bssl::CertificateTrust trust = trust_store_chrome->GetTrust(parsed.get());
    EXPECT_TRUE(trust.IsTrustAnchor());
    // Count how many certs are on the EUTL.
    bssl::CertificateTrust eutl_trust =
        trust_store_chrome->eutl_trust_store()->GetTrust(parsed.get());
    if (eutl_trust.type == bssl::CertificateTrustType::TRUSTED_ANCHOR) {
      eutl_certs++;
    }
  }
  // There should be one cert from test_store.certs on the EUTL.
  EXPECT_EQ(eutl_certs, 1);

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

TEST(TrustStoreChromeTestNoFixture, ContainsEutlCert) {
  std::unique_ptr<TrustStoreChrome> trust_store_chrome =
      TrustStoreChrome::CreateTrustStoreForTesting(
          base::span<const ChromeRootCertInfo>(kChromeRootCertList),
          base::span(kEutlRootCertList),
          /*version=*/1);

  const std::string kEUTLCertHash =
      "f7c7e28fb5e79f314aaac6bbba932f15e1a72069f435d4c9e707f93ca1482ee3";

  // Check that the EUTL certificate in test_additional.certs is included in
  // the EUTL trust store, but not trusted for TLS connection establishment.
  CertificateList certs = CreateCertificateListFromFile(
      GetTestNetDataDirectory().AppendASCII("ssl/chrome_root_store"),
      "test_additional.certs", X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  std::shared_ptr<const bssl::ParsedCertificate> parsed =
      FindParsedCertificateInCertificateList(kEUTLCertHash, certs);
  ASSERT_TRUE(parsed);

  bssl::CertificateTrust eutl_trust =
      trust_store_chrome->eutl_trust_store()->GetTrust(parsed.get());
  EXPECT_EQ(bssl::CertificateTrust::ForTrustAnchor().ToDebugString(),
            eutl_trust.ToDebugString());

  EXPECT_FALSE(trust_store_chrome->Contains(parsed.get()));
  bssl::CertificateTrust trust = trust_store_chrome->GetTrust(parsed.get());
  EXPECT_EQ(bssl::CertificateTrust::ForUnspecified().ToDebugString(),
            trust.ToDebugString());

  // Other certificates should not be included. Which test cert used here isn't
  // important as long as it isn't one of the certificates in the
  // chrome_root_store/test_store.certs.
  scoped_refptr<X509Certificate> other_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(other_cert);
  std::shared_ptr<const bssl::ParsedCertificate> other_parsed =
      ToParsedCertificate(*other_cert);
  eutl_trust =
      trust_store_chrome->eutl_trust_store()->GetTrust(other_parsed.get());
  EXPECT_EQ(bssl::CertificateTrust::ForUnspecified().ToDebugString(),
            eutl_trust.ToDebugString());
}

TEST(TrustStoreChromeTestNoFixture, Constraints) {
  std::unique_ptr<TrustStoreChrome> trust_store_chrome =
      TrustStoreChrome::CreateTrustStoreForTesting(
          base::span<const ChromeRootCertInfo>(kChromeRootCertList),
          base::span(kEutlRootCertList),
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
  for (const auto& cert : certs) {
    std::shared_ptr<const bssl::ParsedCertificate> parsed =
        ToParsedCertificate(*cert);
    std::string sha256_hex =
        base::HexEncodeLower(crypto::SHA256Hash(parsed->der_cert()));
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
  ASSERT_EQ(constraints.size(), 3U);

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
  EXPECT_THAT(constraints[0].permitted_dns_names,
              testing::ElementsAre("foo.example.com", "bar.example.com"));

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
  EXPECT_TRUE(constraints[1].permitted_dns_names.empty());

  EXPECT_THAT(constraints[2].permitted_dns_names,
              testing::ElementsAre("baz.example.com"));

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

// TODO(crbug.com/452986179): test MTC anchor constraints, etc, once
// implemented.

TEST(TrustStoreChromeTestNoFixture, EnforceAnchorExpiryAndConstraints) {
  std::unique_ptr<TrustStoreChrome> trust_store_chrome =
      TrustStoreChrome::CreateTrustStoreForTesting(
          base::span<const ChromeRootCertInfo>(kChromeRootCertList),
          base::span(kEutlRootCertList),
          /*version=*/1);

  std::map<std::string /* SHA-256 hash of certificate */,
           bssl::CertificateTrust>
      tests = {
          {"568d6905a2c88708a4b3025190edcfedb1974a606a13c6e5290fcb2ae63edab5",
           bssl::CertificateTrust::ForTrustAnchor()},
          {"d92e93252eabca950870b94331990963a2dd5db96d833c82b08e41afd1719178",
           bssl::CertificateTrust::ForTrustAnchor().WithEnforceAnchorExpiry()},
          {"68b9c761219a5b1f0131784474665db61bbdb109e00f05ca9f74244ee5f5f52b",
           bssl::CertificateTrust::ForTrustAnchor()
               .WithEnforceAnchorConstraints()},
          {"687fa451382278fff0c8b11f8d43d576671c6eb2bceab413fb83d965d06d2ff2",
           bssl::CertificateTrust::ForTrustAnchor()
               .WithEnforceAnchorExpiry()
               .WithEnforceAnchorConstraints()},
      };

  CertificateList certs = CreateCertificateListFromFile(
      GetTestNetDataDirectory().AppendASCII("ssl/chrome_root_store"),
      "test_store.certs", X509Certificate::FORMAT_PEM_CERT_SEQUENCE);

  for (const auto& test : tests) {
    std::shared_ptr<const bssl::ParsedCertificate> cert =
        FindParsedCertificateInCertificateList(test.first, certs);
    bssl::CertificateTrust trust = trust_store_chrome->GetTrust(cert.get());
    EXPECT_TRUE(trust.IsTrustAnchor());
    EXPECT_EQ(trust.enforce_anchor_expiry, test.second.enforce_anchor_expiry);
    EXPECT_EQ(trust.enforce_anchor_constraints,
              test.second.enforce_anchor_constraints);
  }
}

TEST(TrustStoreChromeTestNoFixture,
     EnforceAnchorExpiryAndConstraintsFromProto) {
  for (bool enforce_anchor_expiry : {false, true}) {
    for (bool enforce_anchor_constraints : {false, true}) {
      scoped_refptr<X509Certificate> root = MakeTestRoot();
      chrome_root_store::RootStore root_store;
      chrome_root_store::TrustAnchor* anchor = root_store.add_trust_anchors();
      anchor->set_der(
          net::x509_util::CryptoBufferAsStringPiece(root->cert_buffer()));
      anchor->set_enforce_anchor_expiry(enforce_anchor_expiry);
      anchor->set_enforce_anchor_constraints(enforce_anchor_constraints);

      std::optional<ChromeRootStoreData> root_store_data =
          ChromeRootStoreData::CreateFromRootStoreProto(root_store);
      ASSERT_TRUE(root_store_data);
      TrustStoreChrome trust_store_chrome(&root_store_data.value(),
                                          /*mtc_metadata=*/nullptr);

      std::shared_ptr<const bssl::ParsedCertificate> parsed =
          ToParsedCertificate(*root);
      bssl::CertificateTrust trust = trust_store_chrome.GetTrust(parsed.get());
      EXPECT_TRUE(trust.IsTrustAnchor());
      EXPECT_EQ(trust.enforce_anchor_expiry, enforce_anchor_expiry);
      EXPECT_EQ(trust.enforce_anchor_constraints, enforce_anchor_constraints);
    }
  }
}

// Tests that, for a compiled-in root store, certificates in |additional_certs|
// are compiled in as trust anchors when indicated, with associated trust anchor
// IDs when present, with |enforce_anchor_expiry| and
// |enforce_anchor_constraints| flags enforced.
TEST(TrustStoreChromeTestNoFixture,
     LoadCompiledTrustAnchorsWithTrustAnchorIDs) {
  std::unique_ptr<TrustStoreChrome> trust_store_chrome =
      TrustStoreChrome::CreateTrustStoreForTesting(
          base::span<const ChromeRootCertInfo>(kChromeRootCertList),
          base::span(kEutlRootCertList),
          /*version=*/1);

  // Map of hex-encoded SHA-256 hashes of |trust_anchors| certificates that have
  // associated Trust Anchor IDs to their expected CertificateTrust setting.
  std::map<std::string, bssl::CertificateTrust>
      expected_trust_anchor_trust_by_hash = {
          {"687fa451382278fff0c8b11f8d43d576671c6eb2bceab413fb83d965d06d2ff2",
           bssl::CertificateTrust::ForTrustAnchor()
               .WithEnforceAnchorExpiry()
               .WithEnforceAnchorConstraints()},
      };

  // Map of hex-encoded SHA-256 hashes of |additional_certs| certificates that
  // are marked as trust anchors, and have associated Trust Anchor IDs, to their
  // expected CertificateTrust setting.
  std::map<std::string, bssl::CertificateTrust>
      expected_additional_certificate_trust_by_hash = {
          {"72a34ac2b424aed3f6b0b04755b88cc027dccc806fddb22b4cd7c47773973ec0",
           bssl::CertificateTrust::ForTrustAnchor().WithEnforceAnchorExpiry()},
          {"e6fe22bf45e4f0d3b85c59e02c0f495418e1eb8d3210f788d48cd5e1cb547cd4",
           bssl::CertificateTrust::ForTrustAnchor()
               .WithEnforceAnchorConstraints()},
          {"973a41276ffd01e027a2aad49e34c37846d3e976ff6a620b6712e33832041aa6",
           bssl::CertificateTrust::ForTrustAnchor()
               .WithEnforceAnchorExpiry()
               .WithEnforceAnchorConstraints()}};

  CertificateList trust_anchor_certs = CreateCertificateListFromFile(
      GetTestNetDataDirectory().AppendASCII("ssl/chrome_root_store"),
      "test_store.certs", X509Certificate::FORMAT_PEM_CERT_SEQUENCE);

  CertificateList additional_certs = CreateCertificateListFromFile(
      GetTestNetDataDirectory().AppendASCII("ssl/chrome_root_store"),
      "test_additional.certs", X509Certificate::FORMAT_PEM_CERT_SEQUENCE);

  size_t certs_with_tai = 0;
  for (const auto& cert : kChromeRootCertList) {
    if (cert.trust_anchor_id.empty()) {
      continue;
    }

    certs_with_tai++;
    std::string hash =
        base::HexEncodeLower(crypto::SHA256Hash(cert.root_cert_der));
    bool is_additional_cert =
        expected_additional_certificate_trust_by_hash.contains(hash);
    bssl::CertificateTrust expected_trust =
        is_additional_cert ? expected_additional_certificate_trust_by_hash[hash]
                           : expected_trust_anchor_trust_by_hash[hash];

    std::shared_ptr<const bssl::ParsedCertificate> parsed_cert =
        is_additional_cert
            ? FindParsedCertificateInCertificateList(hash, additional_certs)
            : FindParsedCertificateInCertificateList(hash, trust_anchor_certs);
    ASSERT_TRUE(parsed_cert);

    // Check that the certificate is present in the trust store as an anchor,
    // with the expected settings for expiry and X.509 constraints.
    // TODO(crbug.com/414630735): check that the correct Trust Anchor ID is
    // stored in TrustStoreChrome, once implemented. (Right now TrustStoreChrome
    // throws out Trust Anchor IDs and doesn't keep them around.)
    bssl::CertificateTrust trust =
        trust_store_chrome->GetTrust(parsed_cert.get());
    EXPECT_TRUE(trust.IsTrustAnchor());
    EXPECT_EQ(trust.enforce_anchor_expiry,
              expected_trust.enforce_anchor_expiry);
    EXPECT_EQ(trust.enforce_anchor_constraints,
              expected_trust.enforce_anchor_constraints);
  }
  EXPECT_EQ(4u, certs_with_tai);
}

// Tests that, for a compiled-in root store, certificates in |additional_certs|
// are compiled in as trust anchors when indicated, even if they have no
// associated Trust Anchor ID.
TEST(TrustStoreChromeTestNoFixture,
     LoadCompiledTrustAnchorsWithNoTrustAnchorID) {
  std::unique_ptr<TrustStoreChrome> trust_store_chrome =
      TrustStoreChrome::CreateTrustStoreForTesting(
          base::span<const ChromeRootCertInfo>(kChromeRootCertList),
          base::span(kEutlRootCertList),
          /*version=*/1);

  const std::string kAdditionalCertTrustAnchorWithNoTAIHash =
      "19400be5b7a31fb733917700789d2f0a2471c0c9d506c0e504c06c16d7cb17c0";

  CertificateList additional_certs = CreateCertificateListFromFile(
      GetTestNetDataDirectory().AppendASCII("ssl/chrome_root_store"),
      "test_additional.certs", X509Certificate::FORMAT_PEM_CERT_SEQUENCE);

  std::shared_ptr<const bssl::ParsedCertificate> parsed_cert =
      FindParsedCertificateInCertificateList(
          kAdditionalCertTrustAnchorWithNoTAIHash, additional_certs);
  ASSERT_TRUE(parsed_cert);
  bssl::CertificateTrust trust =
      trust_store_chrome->GetTrust(parsed_cert.get());
  EXPECT_TRUE(trust.IsTrustAnchor());
  EXPECT_TRUE(trust.enforce_anchor_expiry);
  EXPECT_TRUE(trust.enforce_anchor_constraints);
}

// Tests that, for a root loaded from a proto, certificates in
// |additional_certs| are loaded into TrustStoreChrome as trust anchors when
// indicated, with |enforce_anchor_expiry| and |enforce_anchor_constraints|
// flags enforced.
TEST(TrustStoreChromeTestNoFixture, LoadProtoAdditionalCertsAsTrustAnchors) {
  for (bool enforce_anchor_expiry : {true, false}) {
    for (bool enforce_anchor_constraints : {true, false}) {
      scoped_refptr<X509Certificate> root = MakeTestRoot();
      chrome_root_store::RootStore root_store;
      chrome_root_store::TrustAnchor* anchor =
          root_store.add_additional_certs();
      anchor->set_der(
          net::x509_util::CryptoBufferAsStringPiece(root->cert_buffer()));
      anchor->set_enforce_anchor_expiry(enforce_anchor_expiry);
      anchor->set_enforce_anchor_constraints(enforce_anchor_constraints);
      anchor->set_tls_trust_anchor(true);
      anchor->set_trust_anchor_id("\x01\x02\x03\x04");

      std::optional<ChromeRootStoreData> root_store_data =
          ChromeRootStoreData::CreateFromRootStoreProto(root_store);
      ASSERT_TRUE(root_store_data);
      TrustStoreChrome trust_store_chrome(&root_store_data.value(),
                                          /*mtc_metadata=*/nullptr);

      std::shared_ptr<const bssl::ParsedCertificate> parsed =
          ToParsedCertificate(*root);
      bssl::CertificateTrust trust = trust_store_chrome.GetTrust(parsed.get());
      EXPECT_TRUE(trust.IsTrustAnchor());
      EXPECT_EQ(trust.enforce_anchor_expiry, enforce_anchor_expiry);
      EXPECT_EQ(trust.enforce_anchor_constraints, enforce_anchor_constraints);
      // TODO(crbug.com/414630735): check that the correct Trust Anchor ID is
      // stored in TrustStoreChrome, once implemented. (Right now
      // TrustStoreChrome throws out Trust Anchor IDs and doesn't keep them
      // around.)
    }
  }
}

// Tests that, for a root loaded from a proto, certificates in
// |additional_certs| are loaded into TrustStoreChrome as trust anchors when
// indicated, even if there is no Trust Anchor ID set.
TEST(TrustStoreChromeTestNoFixture,
     LoadProtoAdditionalCertsAsTrustAnchorsWithNoTrustAnchorID) {
  scoped_refptr<X509Certificate> root = MakeTestRoot();
  chrome_root_store::RootStore root_store;
  chrome_root_store::TrustAnchor* anchor = root_store.add_additional_certs();
  anchor->set_der(
      net::x509_util::CryptoBufferAsStringPiece(root->cert_buffer()));
  anchor->set_enforce_anchor_expiry(true);
  anchor->set_enforce_anchor_constraints(true);
  anchor->set_tls_trust_anchor(true);
  // `trust_anchor_id` is left unset here.

  std::optional<ChromeRootStoreData> root_store_data =
      ChromeRootStoreData::CreateFromRootStoreProto(root_store);
  ASSERT_TRUE(root_store_data);
  TrustStoreChrome trust_store_chrome(&root_store_data.value(),
                                      /*mtc_metadata=*/nullptr);

  std::shared_ptr<const bssl::ParsedCertificate> parsed =
      ToParsedCertificate(*root);
  bssl::CertificateTrust trust = trust_store_chrome.GetTrust(parsed.get());
  EXPECT_TRUE(trust.IsTrustAnchor());
  EXPECT_TRUE(trust.enforce_anchor_expiry);
  EXPECT_TRUE(trust.enforce_anchor_constraints);
}

// Tests that, for a root loaded from a proto, certificates in
// |additional_certs| are not loaded into TrustStoreChrome as trust anchors when
// |tls_trust_anchor| is false.
TEST(TrustStoreChromeTestNoFixture, LoadProtoNonAnchorsAreNotTrusted) {
  scoped_refptr<X509Certificate> root = MakeTestRoot();
  chrome_root_store::RootStore root_store;
  chrome_root_store::TrustAnchor* anchor = root_store.add_additional_certs();
  anchor->set_der(
      net::x509_util::CryptoBufferAsStringPiece(root->cert_buffer()));
  anchor->set_enforce_anchor_expiry(true);
  anchor->set_enforce_anchor_constraints(true);
  // |tls_trust_anchor| is left unset here.
  anchor->set_trust_anchor_id("\x01\x02\x03\x04");

  std::optional<ChromeRootStoreData> root_store_data =
      ChromeRootStoreData::CreateFromRootStoreProto(root_store);
  ASSERT_TRUE(root_store_data);
  TrustStoreChrome trust_store_chrome(&root_store_data.value(),
                                      /*mtc_metadata=*/nullptr);

  std::shared_ptr<const bssl::ParsedCertificate> parsed =
      ToParsedCertificate(*root);
  EXPECT_FALSE(trust_store_chrome.Contains(parsed.get()));
  // TODO(crbug.com/414630735): check that the above Trust Anchor ID is
  // not present in TrustStoreChrome, once implemented. (Right now
  // TrustStoreChrome throws out Trust Anchor IDs and doesn't keep them around.)
}

// Tests that TLS Trust Anchor IDs are loaded correctly from the compiled-in
// root store.
TEST(TrustStoreChromeTestNoFixture, LoadCompiledInTrustAnchorIDs) {
  std::vector<std::vector<uint8_t>> trust_anchor_ids =
      TrustStoreChrome::GetTrustAnchorIDsFromCompiledInRootStore(
          base::span<const ChromeRootCertInfo>(kChromeRootCertList));
  EXPECT_THAT(trust_anchor_ids,
              testing::UnorderedElementsAre(
                  std::vector<uint8_t>({0x05u, 0x05u, 0x05u}),
                  std::vector<uint8_t>({0x01u, 0x01u, 0x01u, 0x01u}),
                  std::vector<uint8_t>({0x03u, 0x03u, 0x03u, 0x03u}),
                  std::vector<uint8_t>({0x02u, 0x02u, 0x02u, 0x02u})));
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
      {std::nullopt,
       std::nullopt,
       std::nullopt,
       /*max_version_exclusive=*/std::make_optional(base::Version("31")),
       {}}};

  override_constraints[crypto::SHA256Hash(root4->cert_span())] = {
      {std::nullopt,
       std::nullopt,
       std::nullopt,
       /*max_version_exclusive=*/std::make_optional(base::Version("41")),
       {}}};

  override_constraints[crypto::SHA256Hash(root6->cert_span())] = {
      {std::nullopt,
       std::nullopt,
       std::nullopt,
       /*max_version_exclusive=*/std::make_optional(base::Version("61")),
       {}}};

  std::unique_ptr<TrustStoreChrome> trust_store_chrome =
      TrustStoreChrome::CreateTrustStoreForTesting(
          std::move(root_cert_info),
          /*eutl_certs=*/{},
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
    auto it = constraints.find(base::span(hash));
    ASSERT_NE(it, constraints.end());
    ASSERT_EQ(it->second.size(), 1U);
    const auto& constraint1 = it->second[0];
    EXPECT_FALSE(constraint1.sct_not_after.has_value());
    EXPECT_FALSE(constraint1.sct_all_after.has_value());
    EXPECT_FALSE(constraint1.min_version.has_value());
    EXPECT_FALSE(constraint1.max_version_exclusive.has_value());
    EXPECT_THAT(constraint1.permitted_dns_names, testing::IsEmpty());
  }
  {
    constexpr uint8_t hash[] = {0xa7, 0xe0, 0xc7, 0x5d, 0x7f, 0x77, 0x2f, 0xcc,
                                0xf2, 0x6a, 0x6a, 0xc1, 0xf7, 0xb0, 0xa8, 0x6a,
                                0x48, 0x2e, 0x2f, 0x3d, 0x32, 0x6b, 0xc9, 0x11,
                                0xc9, 0x5d, 0x56, 0xff, 0x3d, 0x49, 0x06, 0xd5};
    auto it = constraints.find(base::span(hash));
    ASSERT_NE(it, constraints.end());
    ASSERT_EQ(it->second.size(), 1U);

    const auto& constraint1 = it->second[0];
    ASSERT_TRUE(constraint1.sct_not_after.has_value());
    EXPECT_EQ(constraint1.sct_not_after->InMillisecondsSinceUnixEpoch() / 1000,
              789012345);
    EXPECT_FALSE(constraint1.sct_all_after.has_value());
    EXPECT_FALSE(constraint1.min_version.has_value());
    EXPECT_FALSE(constraint1.max_version_exclusive.has_value());
    EXPECT_THAT(constraint1.permitted_dns_names, testing::IsEmpty());
  }

  {
    unsigned char hash[] = {0x65, 0xee, 0x41, 0xe8, 0xa8, 0xc2, 0x7b, 0x71,
                            0xb6, 0xbf, 0xcf, 0x44, 0x65, 0x3c, 0x8e, 0x83,
                            0x70, 0xec, 0x5e, 0x10, 0x6e, 0x27, 0x25, 0x92,
                            0xc2, 0xfb, 0xcb, 0xad, 0xf8, 0xdc, 0x57, 0x63};

    auto it = constraints.find(base::span(hash));
    ASSERT_NE(it, constraints.end());
    ASSERT_EQ(it->second.size(), 1U);
    const auto& constraint = it->second[0];
    ASSERT_TRUE(constraint.sct_not_after.has_value());
    EXPECT_EQ(constraint.sct_not_after->InMillisecondsSinceUnixEpoch() / 1000,
              123456);
    EXPECT_FALSE(constraint.sct_all_after.has_value());
    EXPECT_FALSE(constraint.min_version.has_value());
    EXPECT_EQ(constraint.max_version_exclusive, base::Version({2, 3}));
    EXPECT_THAT(constraint.permitted_dns_names, testing::IsEmpty());
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

  auto it = constraints.find(base::span(hash));
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
      "minversion=1.2.3.4,maxversionexclusive=10,"
      "dns=foo.com,dns=bar.com+"
      "a7e0c75d7f772fccf26a6ac1f7b0a86a482e2f3d326bc911c95d56ff3d4906d5,"
      "568c8ef6b526d1394bca052ba3e4d1f4d7a8d9c88c55a1a9ab7ca0fae2dc5473:"
      "sctallafter=9876543,sctnotafter=1234567890");
  EXPECT_EQ(constraints.size(), 3U);

  {
    constexpr uint8_t hash1[] = {
        0x78, 0x4e, 0xca, 0xa8, 0xb9, 0xdf, 0xcc, 0x82, 0x65, 0x47, 0xf8,
        0x06, 0xf7, 0x59, 0xab, 0xd6, 0xb4, 0x48, 0x15, 0x82, 0xfc, 0x7e,
        0x37, 0x7d, 0xc3, 0xe6, 0xa0, 0xa9, 0x59, 0x02, 0x51, 0x26};
    auto it = constraints.find(base::span(hash1));
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
    EXPECT_THAT(constraint1.permitted_dns_names,
                testing::ElementsAre("foo.com", "bar.com"));
  }

  {
    constexpr uint8_t hash2[] = {
        0xa7, 0xe0, 0xc7, 0x5d, 0x7f, 0x77, 0x2f, 0xcc, 0xf2, 0x6a, 0x6a,
        0xc1, 0xf7, 0xb0, 0xa8, 0x6a, 0x48, 0x2e, 0x2f, 0x3d, 0x32, 0x6b,
        0xc9, 0x11, 0xc9, 0x5d, 0x56, 0xff, 0x3d, 0x49, 0x06, 0xd5};
    auto it = constraints.find(base::span(hash2));
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
    EXPECT_THAT(constraint1.permitted_dns_names,
                testing::ElementsAre("foo.com", "bar.com"));

    const auto& constraint2 = it->second[1];
    ASSERT_TRUE(constraint2.sct_not_after.has_value());
    EXPECT_EQ(constraint2.sct_not_after->InMillisecondsSinceUnixEpoch() / 1000,
              1234567890);
    ASSERT_TRUE(constraint2.sct_all_after.has_value());
    EXPECT_EQ(constraint2.sct_all_after->InMillisecondsSinceUnixEpoch() / 1000,
              9876543);
    EXPECT_FALSE(constraint2.min_version.has_value());
    EXPECT_FALSE(constraint2.max_version_exclusive.has_value());
    EXPECT_THAT(constraint2.permitted_dns_names, testing::IsEmpty());
  }

  {
    constexpr uint8_t hash3[] = {
        0x56, 0x8c, 0x8e, 0xf6, 0xb5, 0x26, 0xd1, 0x39, 0x4b, 0xca, 0x05,
        0x2b, 0xa3, 0xe4, 0xd1, 0xf4, 0xd7, 0xa8, 0xd9, 0xc8, 0x8c, 0x55,
        0xa1, 0xa9, 0xab, 0x7c, 0xa0, 0xfa, 0xe2, 0xdc, 0x54, 0x73};
    auto it = constraints.find(base::span(hash3));
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
    EXPECT_THAT(constraint1.permitted_dns_names, testing::IsEmpty());
  }
}

}  // namespace
}  // namespace net
