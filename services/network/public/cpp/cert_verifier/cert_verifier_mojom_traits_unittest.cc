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

}  // namespace cert_verifier
