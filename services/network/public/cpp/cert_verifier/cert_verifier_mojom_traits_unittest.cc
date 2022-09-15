// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cert_verifier/cert_verifier_mojom_traits.h"
#include <algorithm>
#include <string>
#include <tuple>

#include "base/files/file_util.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/hash_value.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/crl_set.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cert_verifier {
TEST(CertVerifierMojomTraitsTest, RequestParams) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  scoped_refptr<net::X509Certificate> test_cert(
      net::ImportCertFromFile(certs_dir, "ok_cert.pem"));

  const std::string hostname = "www.example.com";
  const int flags = net::CertVerifier::VERIFY_DISABLE_NETWORK_FETCHES;
  const std::string ocsp_response = "i_am_an_ocsp_response";
  const std::string sct_list = "and_i_am_an_sct_list!";

  net::CertVerifier::RequestParams params(test_cert, hostname, flags,
                                          ocsp_response, sct_list);
  net::CertVerifier::RequestParams out_params;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::RequestParams>(
      params, out_params));

  ASSERT_EQ(params, out_params);
}

namespace {
bool CRLSetsEqual(scoped_refptr<const net::CRLSet> crl_set1,
                  scoped_refptr<const net::CRLSet> crl_set2) {
  if (crl_set1 == nullptr || crl_set2 == nullptr)
    return crl_set1 == nullptr && crl_set2 == nullptr;

  const net::CRLSet::CRLList& crl_list1 = crl_set1->CrlsForTesting();
  const net::CRLSet::CRLList& crl_list2 = crl_set2->CrlsForTesting();
  if (crl_list1.size() != crl_list2.size())
    return false;

  for (const auto& pair : crl_list1) {
    if (crl_list2.count(pair.first) == 0)
      return false;

    std::vector<std::string> revoked_serial_nums1 = pair.second;
    std::vector<std::string> revoked_serial_nums2 = crl_list2.at(pair.first);
    if (revoked_serial_nums1.size() != revoked_serial_nums2.size())
      return false;

    std::sort(revoked_serial_nums1.begin(), revoked_serial_nums1.end());
    std::sort(revoked_serial_nums2.begin(), revoked_serial_nums2.end());
    if (revoked_serial_nums1 != revoked_serial_nums2)
      return false;
  }

  return true;
}

bool CertificateListsEqual(const net::CertificateList& cert_list1,
                           const net::CertificateList& cert_list2) {
  if (cert_list1.size() != cert_list2.size())
    return false;

  std::set<net::SHA256HashValue> fingerprints1, fingerprints2;
  for (size_t i = 0; i < cert_list1.size(); i++) {
    fingerprints1.insert(cert_list1[i]->CalculateChainFingerprint256());
    fingerprints2.insert(cert_list2[i]->CalculateChainFingerprint256());
  }

  return fingerprints1 == fingerprints2;
}

bool ConfigsEqual(const net::CertVerifier::Config& config1,
                  const net::CertVerifier::Config& config2) {
  if (std::tie(config1.enable_rev_checking,
               config1.require_rev_checking_local_anchors,
               config1.enable_sha1_local_anchors,
               config1.disable_symantec_enforcement) !=
      std::tie(config2.enable_rev_checking,
               config2.require_rev_checking_local_anchors,
               config2.enable_sha1_local_anchors,
               config2.disable_symantec_enforcement))
    return false;

  if (!CRLSetsEqual(config1.crl_set, config2.crl_set))
    return false;

  if (!CertificateListsEqual(config1.additional_trust_anchors,
                             config2.additional_trust_anchors))
    return false;

  if (!CertificateListsEqual(config1.additional_untrusted_authorities,
                             config2.additional_untrusted_authorities))
    return false;

  return true;
}
}  // namespace

TEST(CertVerifierMojomTraitsTest, ConfigBasic) {
  net::CertVerifier::Config config;
  net::CertVerifier::Config out_config;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CertVerifierConfig>(
      config, out_config));
  ASSERT_TRUE(ConfigsEqual(config, out_config));
}

TEST(CertVerifierMojomTraitsTest, ConfigTrue) {
  net::CertVerifier::Config config;
  config.enable_rev_checking = true;
  config.require_rev_checking_local_anchors = true;
  config.enable_sha1_local_anchors = true;
  config.disable_symantec_enforcement = true;

  net::CertVerifier::Config out_config;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CertVerifierConfig>(
      config, out_config));
  ASSERT_TRUE(ConfigsEqual(config, out_config));
}

TEST(CertVerifierMojomTraitsTest, ConfigCRLAndAdditionalCerts) {
  std::string crl_set;

  base::ReadFileToString(
      net::GetTestCertsDirectory().AppendASCII("crlset_by_leaf_spki.raw"),
      &crl_set);

  const base::FilePath certs_dir = net::GetTestCertsDirectory();

  net::CertVerifier::Config config;
  ASSERT_TRUE(net::CRLSet::ParseAndStoreUnparsedData(crl_set, &config.crl_set));
  config.additional_trust_anchors.push_back(
      net::ImportCertFromFile(certs_dir, "root_ca_cert.pem"));
  config.additional_trust_anchors.push_back(
      net::ImportCertFromFile(certs_dir, "2048-rsa-root.pem"));

  config.additional_untrusted_authorities.push_back(
      net::ImportCertFromFile(certs_dir, "intermediate_ca_cert.pem"));

  net::CertVerifier::Config out_config;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CertVerifierConfig>(
      config, out_config));
  ASSERT_TRUE(ConfigsEqual(config, out_config));
}

}  // namespace cert_verifier
