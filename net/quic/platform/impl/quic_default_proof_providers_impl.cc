// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/platform/impl/quic_default_proof_providers_impl.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/http/transport_security_state.h"
#include "net/quic/crypto/proof_source_chromium.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_ptr_util.h"

DEFINE_QUIC_COMMAND_LINE_FLAG(
    bool,
    allow_unknown_root_cert,
    false,
    "If true, don't restrict cert verification to known roots");

DEFINE_QUIC_COMMAND_LINE_FLAG(std::string,
                              certificate_file,
                              "",
                              "Path to the certificate chain.");

DEFINE_QUIC_COMMAND_LINE_FLAG(std::string,
                              key_file,
                              "",
                              "Path to the pkcs8 private key.");

using net::CertVerifier;
using net::CTVerifier;
using net::MultiLogCTVerifier;
using net::ProofVerifierChromium;

namespace quic {

namespace {

std::set<std::string> UnknownRootAllowlistForHost(std::string host) {
  if (!GetQuicFlag(FLAGS_allow_unknown_root_cert)) {
    return std::set<std::string>();
  }
  return {host};
}

}  // namespace

class ProofVerifierChromiumWithOwnership : public net::ProofVerifierChromium {
 public:
  ProofVerifierChromiumWithOwnership(
      std::unique_ptr<net::CertVerifier> cert_verifier,
      std::string host)
      : net::ProofVerifierChromium(cert_verifier.get(),
                                   &ct_policy_enforcer_,
                                   &transport_security_state_,
                                   &ct_verifier_,
                                   UnknownRootAllowlistForHost(host)),
        cert_verifier_(std::move(cert_verifier)) {}

 private:
  std::unique_ptr<net::CertVerifier> cert_verifier_;
  net::DefaultCTPolicyEnforcer ct_policy_enforcer_;
  net::TransportSecurityState transport_security_state_;
  net::MultiLogCTVerifier ct_verifier_;
};

std::unique_ptr<ProofVerifier> CreateDefaultProofVerifierImpl(
    const std::string& host) {
  std::unique_ptr<net::CertVerifier> cert_verifier =
      net::CertVerifier::CreateDefault(/*cert_net_fetcher=*/nullptr);
  return std::make_unique<ProofVerifierChromiumWithOwnership>(
      std::move(cert_verifier), host);
}

std::unique_ptr<ProofSource> CreateDefaultProofSourceImpl() {
  auto proof_source = std::make_unique<net::ProofSourceChromium>();
  CHECK(proof_source->Initialize(
#if defined(OS_WIN)
      base::FilePath(base::UTF8ToUTF16(GetQuicFlag(FLAGS_certificate_file))),
      base::FilePath(base::UTF8ToUTF16(GetQuicFlag(FLAGS_key_file))),
      base::FilePath()));
#else
      base::FilePath(GetQuicFlag(FLAGS_certificate_file)),
      base::FilePath(GetQuicFlag(FLAGS_key_file)), base::FilePath()));
#endif
  return std::move(proof_source);
}

}  // namespace quic
