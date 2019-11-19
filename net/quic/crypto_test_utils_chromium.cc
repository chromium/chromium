// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/crypto/proof_source_chromium.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/ssl/ssl_config_service.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/third_party/quiche/src/quic/core/crypto/crypto_utils.h"
#include "net/third_party/quiche/src/quic/test_tools/crypto_test_utils.h"

using std::string;

namespace net {

namespace test {

namespace {

class TestProofVerifierChromium : public ProofVerifierChromium {
 public:
  TestProofVerifierChromium(
      std::unique_ptr<CertVerifier> cert_verifier,
      std::unique_ptr<TransportSecurityState> transport_security_state,
      std::unique_ptr<CTVerifier> cert_transparency_verifier,
      std::unique_ptr<CTPolicyEnforcer> ct_policy_enforcer,
      const std::string& cert_file)
      : ProofVerifierChromium(cert_verifier.get(),
                              ct_policy_enforcer.get(),
                              transport_security_state.get(),
                              cert_transparency_verifier.get(),
                              {"test.example.com"}),
        cert_verifier_(std::move(cert_verifier)),
        transport_security_state_(std::move(transport_security_state)),
        cert_transparency_verifier_(std::move(cert_transparency_verifier)),
        ct_policy_enforcer_(std::move(ct_policy_enforcer)) {
    // Load and install the root for the validated chain.
    scoped_refptr<X509Certificate> root_cert =
        ImportCertFromFile(GetTestCertsDirectory(), cert_file);
    scoped_root_.Reset({root_cert});
  }

  ~TestProofVerifierChromium() override {}

  CertVerifier* cert_verifier() { return cert_verifier_.get(); }

 private:
  ScopedTestRoot scoped_root_;
  std::unique_ptr<CertVerifier> cert_verifier_;
  std::unique_ptr<TransportSecurityState> transport_security_state_;
  std::unique_ptr<CTVerifier> cert_transparency_verifier_;
  std::unique_ptr<CTPolicyEnforcer> ct_policy_enforcer_;
};

}  // namespace
}  // namespace test
}  // namespace net

namespace quic {
namespace test {
namespace crypto_test_utils {

std::unique_ptr<quic::ProofSource> ProofSourceForTesting() {
  std::unique_ptr<net::ProofSourceChromium> source(
      new net::ProofSourceChromium());
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  CHECK(source->Initialize(certs_dir.AppendASCII("quic-chain.pem"),
                           certs_dir.AppendASCII("quic-leaf-cert.key"),
                           certs_dir.AppendASCII("quic-leaf-cert.key.sct")));
  return std::move(source);
}

std::unique_ptr<quic::ProofVerifier> ProofVerifierForTesting() {
  // TODO(rch): use a real cert verifier?
  std::unique_ptr<net::MockCertVerifier> cert_verifier(
      new net::MockCertVerifier());
  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "quic-chain.pem");
  cert_verifier->AddResultForCertAndHost(verify_result.verified_cert.get(),
                                         "test.example.com", verify_result,
                                         net::OK);
  return std::make_unique<net::test::TestProofVerifierChromium>(
      std::move(cert_verifier), std::make_unique<net::TransportSecurityState>(),
      std::make_unique<net::MultiLogCTVerifier>(),
      std::make_unique<net::DefaultCTPolicyEnforcer>(), "quic-root.pem");
}

std::unique_ptr<quic::ProofVerifyContext> ProofVerifyContextForTesting() {
  return std::make_unique<net::ProofVerifyContextChromium>(
      /*cert_verify_flags=*/0, net::NetLogWithSource());
}

}  // namespace crypto_test_utils

}  // namespace test

}  // namespace quic
