// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/system_trust_store.h"

#include "base/strings/string_view_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/net_buildflags.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/chrome_root_store_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/span.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"
#include "third_party/boringssl/src/pki/signature_algorithm.h"
#include "third_party/boringssl/src/pki/trust_store.h"

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include <vector>

#include "net/cert/internal/platform_trust_store.h"
#include "net/cert/internal/trust_store_chrome.h"
#include "net/cert/root_store_proto_lite/root_store.pb.h"
#endif  // CHROME_ROOT_STORE_SUPPORTED

namespace net {

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "net/data/ssl/chrome_root_store/chrome-root-store-test-data-inc.cc"  // nogncheck

class TestPlatformTrustStore : public PlatformTrustStore {
 public:
  explicit TestPlatformTrustStore(std::unique_ptr<bssl::TrustStore> trust_store)
      : trust_store_(std::move(trust_store)) {}

  void SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                        bssl::ParsedCertificateList* issuers) override {
    trust_store_->SyncGetIssuersOf(cert, issuers);
  }

  // bssl::TrustStore implementation:
  bssl::CertificateTrust GetTrust(
      const bssl::ParsedCertificate* cert) override {
    return trust_store_->GetTrust(cert);
  }

  // net::PlatformTrustStore implementation:
  std::vector<net::PlatformTrustStore::CertWithTrust> GetAllUserAddedCerts()
      override {
    return {};
  }

 private:
  std::unique_ptr<bssl::TrustStore> trust_store_;
};

TEST(SystemTrustStoreChrome, SystemDistrustOverridesChromeTrust) {
  CertificateList certs = CreateCertificateListFromFile(
      GetTestNetDataDirectory().AppendASCII("ssl/chrome_root_store"),
      "test_store.certs", X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_GE(certs.size(), 1u);

  std::shared_ptr<const bssl::ParsedCertificate> root =
      bssl::ParsedCertificate::Create(
          bssl::UpRef(certs[0]->cert_buffer()),
          x509_util::DefaultParseCertificateOptions(), nullptr);
  ASSERT_TRUE(root);

  auto test_system_trust_store = std::make_unique<bssl::TrustStoreInMemory>();
  auto* test_system_trust_store_ptr = test_system_trust_store.get();

  std::unique_ptr<TrustStoreChrome> test_trust_store_chrome =
      TrustStoreChrome::CreateTrustStoreForTesting(
          base::span<const ChromeRootCertInfo>(kChromeRootCertList),
          base::span(kEutlRootCertList),
          /*version=*/1);

  std::unique_ptr<net::PlatformTrustStore> test_platform_trust_store =
      std::make_unique<TestPlatformTrustStore>(
          std::move(test_system_trust_store));

  std::unique_ptr<SystemTrustStore> system_trust_store_chrome =
      CreateSystemTrustStoreChromeForTesting(
          std::move(test_trust_store_chrome),
          std::move(test_platform_trust_store));

  // With no trust settings in the fake system trust store, the cert is trusted
  // by the test chrome root store.
  EXPECT_TRUE(system_trust_store_chrome->GetTrustStore()
                  ->GetTrust(root.get())
                  .IsTrustAnchor());

  // Adding a distrust entry in the fake system trust store should override the
  // trust in the chrome root store.
  test_system_trust_store_ptr->AddDistrustedCertificateForTest(root);
  EXPECT_TRUE(system_trust_store_chrome->GetTrustStore()
                  ->GetTrust(root.get())
                  .IsDistrusted());
}

TEST(SystemTrustStoreChrome, SystemLeafTrustDoesNotOverrideChromeTrust) {
  CertificateList certs = CreateCertificateListFromFile(
      GetTestNetDataDirectory().AppendASCII("ssl/chrome_root_store"),
      "test_store.certs", X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_GE(certs.size(), 1u);

  std::shared_ptr<const bssl::ParsedCertificate> root =
      bssl::ParsedCertificate::Create(
          bssl::UpRef(certs[0]->cert_buffer()),
          x509_util::DefaultParseCertificateOptions(), nullptr);
  ASSERT_TRUE(root);

  auto test_system_trust_store = std::make_unique<bssl::TrustStoreInMemory>();
  auto* test_system_trust_store_ptr = test_system_trust_store.get();

  std::unique_ptr<TrustStoreChrome> test_trust_store_chrome =
      TrustStoreChrome::CreateTrustStoreForTesting(
          base::span<const ChromeRootCertInfo>(kChromeRootCertList),
          base::span(kEutlRootCertList),
          /*version=*/1);

  std::unique_ptr<net::PlatformTrustStore> test_platform_trust_store =
      std::make_unique<TestPlatformTrustStore>(
          std::move(test_system_trust_store));

  std::unique_ptr<SystemTrustStore> system_trust_store_chrome =
      CreateSystemTrustStoreChromeForTesting(
          std::move(test_trust_store_chrome),
          std::move(test_platform_trust_store));

  // With no trust settings in the fake system trust store, the cert is trusted
  // by the test chrome root store.
  EXPECT_TRUE(system_trust_store_chrome->GetTrustStore()
                  ->GetTrust(root.get())
                  .IsTrustAnchor());

  // Adding the certificate to the fake system store as a trusted leaf doesn't
  // matter, the trust in the chrome root store is still preferred.
  test_system_trust_store_ptr->AddCertificate(
      root, bssl::CertificateTrust::ForTrustedLeaf());
  EXPECT_TRUE(system_trust_store_chrome->GetTrustStore()
                  ->GetTrust(root.get())
                  .IsTrustAnchor());
  EXPECT_FALSE(system_trust_store_chrome->GetTrustStore()
                   ->GetTrust(root.get())
                   .IsTrustLeaf());
}

TEST(SystemTrustStoreChrome, DefaultVersions) {
  std::unique_ptr<SystemTrustStore> system_trust_store =
      CreateChromeOnlySystemTrustStore(std::make_unique<TrustStoreChrome>());

  EXPECT_EQ(system_trust_store->chrome_root_store_version(),
            net::CompiledChromeRootStoreVersion());
  EXPECT_EQ(system_trust_store->mtc_metadata_update_time(), std::nullopt);
}

TEST(SystemTrustStoreChrome, KnownRootsFromRootStoreProto) {
  base::test::ScopedFeatureList scoped_feature_list{features::kVerifyMTCs};

  static constexpr uint8_t kMtcCaId[] = {0x09, 0x08, 0x07};
  static constexpr uint8_t kMtcCaId2[] = {0x02, 0x03, 0x04};
  // The MTC CA key doesn't need to be parsable for this test.
  static constexpr uint8_t kFakeMtcKey[] = {1};
  int64_t crs_version = net::CompiledChromeRootStoreVersion();
  auto [unused_leaf, root] = net::CertBuilder::CreateSimpleChain2();

  chrome_root_store::RootStore root_store_proto;
  root_store_proto.set_version_major(++crs_version);

  chrome_root_store::TrustAnchor* anchor = root_store_proto.add_trust_anchors();
  anchor->set_der(root->GetDER());
  std::optional<ChromeRootStoreData> root_store_data =
      ChromeRootStoreData::CreateFromRootStoreProto(root_store_proto);
  ASSERT_TRUE(root_store_data);

  chrome_root_store::SignerSet signer_set;
  AddSignerSetIssuer(signer_set, kMtcCaId, "op1", std::nullopt);
  std::optional<ChromeRootStoreSignerSet> parsed_set =
      ChromeRootStoreSignerSet::CreateFromProto(signer_set);
  ASSERT_TRUE(parsed_set);
  root_store_data->SetSignerSet(*parsed_set);

  std::unique_ptr<SystemTrustStore> system_trust_store =
      CreateChromeOnlySystemTrustStore(std::make_unique<TrustStoreChrome>(
          &*root_store_data, /*mtc_metadata=*/nullptr));

  EXPECT_EQ(system_trust_store->chrome_root_store_version(), crs_version);

  {
    // The traditional anchor and MTC anchor that were added from the protos
    // should be recognized as known roots.
    std::shared_ptr<const bssl::ParsedCertificate> parsed_root =
        bssl::ParsedCertificate::Create(
            bssl::UpRef(root->GetCertBuffer()),
            x509_util::DefaultParseCertificateOptions(), nullptr);
    ASSERT_TRUE(parsed_root);
    EXPECT_TRUE(system_trust_store->IsKnownRoot(parsed_root.get()));

    std::shared_ptr<const bssl::MTCAnchor> mtc_anchor =
        std::make_shared<bssl::MTCAnchor>(
            bssl::MakeSpan(kMtcCaId), bssl::SignatureAlgorithm::kMldsa44,
            x509_util::CreateCryptoBuffer(kFakeMtcKey),
            std::map<uint16_t, std::vector<bssl::TrustedSubtree>>());
    EXPECT_TRUE(system_trust_store->IsKnownMtcAnchor(mtc_anchor.get()));
  }

  {
    // A different anchor and MTC anchor should not be known roots.
    auto [unused_leaf2, root2] = net::CertBuilder::CreateSimpleChain2();
    std::shared_ptr<const bssl::ParsedCertificate> parsed_root2 =
        bssl::ParsedCertificate::Create(
            bssl::UpRef(root2->GetCertBuffer()),
            x509_util::DefaultParseCertificateOptions(), nullptr);
    ASSERT_TRUE(parsed_root2);
    EXPECT_FALSE(system_trust_store->IsKnownRoot(parsed_root2.get()));

    std::shared_ptr<const bssl::MTCAnchor> mtc_anchor2 =
        std::make_shared<bssl::MTCAnchor>(
            bssl::MakeSpan(kMtcCaId2), bssl::SignatureAlgorithm::kMldsa44,
            x509_util::CreateCryptoBuffer(kFakeMtcKey),
            std::map<uint16_t, std::vector<bssl::TrustedSubtree>>());
    EXPECT_FALSE(system_trust_store->IsKnownMtcAnchor(mtc_anchor2.get()));
  }
}
#endif  // CHROME_ROOT_STORE_SUPPORTED
        //
}  // namespace net
