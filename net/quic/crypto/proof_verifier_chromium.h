// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_PROOF_VERIFIER_CHROMIUM_H_
#define NET_QUIC_CRYPTO_PROOF_VERIFIER_CHROMIUM_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_verify_result.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log_with_source.h"
#include "net/third_party/quiche/src/quic/core/crypto/proof_verifier.h"

namespace net {

class CTPolicyEnforcer;
class CertVerifier;
class CTVerifier;
class TransportSecurityState;

// ProofVerifyDetailsChromium is the implementation-specific information that a
// ProofVerifierChromium returns about a certificate verification.
class NET_EXPORT_PRIVATE ProofVerifyDetailsChromium
    : public quic::ProofVerifyDetails {
 public:
  ProofVerifyDetailsChromium();
  ProofVerifyDetailsChromium(const ProofVerifyDetailsChromium&);
  ~ProofVerifyDetailsChromium() override;

  // quic::ProofVerifyDetails implementation
  quic::ProofVerifyDetails* Clone() const override;

  CertVerifyResult cert_verify_result;
  ct::CTVerifyResult ct_verify_result;

  // pinning_failure_log contains a message produced by
  // TransportSecurityState::PKPState::CheckPublicKeyPins in the event of a
  // pinning failure. It is a (somewhat) human-readable string.
  std::string pinning_failure_log;

  // True if PKP was bypassed due to a local trust anchor.
  bool pkp_bypassed;

  // True if there was a certificate error which should be treated as fatal,
  // and false otherwise.
  bool is_fatal_cert_error;
};

// ProofVerifyContextChromium is the implementation-specific information that a
// ProofVerifierChromium needs in order to log correctly.
struct ProofVerifyContextChromium : public quic::ProofVerifyContext {
 public:
  ProofVerifyContextChromium(int cert_verify_flags,
                             const NetLogWithSource& net_log)
      : cert_verify_flags(cert_verify_flags), net_log(net_log) {}

  int cert_verify_flags;
  NetLogWithSource net_log;
};

// ProofVerifierChromium implements the QUIC quic::ProofVerifier interface.  It
// is capable of handling multiple simultaneous requests.
class NET_EXPORT_PRIVATE ProofVerifierChromium : public quic::ProofVerifier {
 public:
  ProofVerifierChromium(CertVerifier* cert_verifier,
                        CTPolicyEnforcer* ct_policy_enforcer,
                        TransportSecurityState* transport_security_state,
                        CTVerifier* cert_transparency_verifier,
                        std::set<std::string> hostnames_to_allow_unknown_roots);
  ~ProofVerifierChromium() override;

  // quic::ProofVerifier interface
  quic::QuicAsyncStatus VerifyProof(
      const std::string& hostname,
      const uint16_t port,
      const std::string& server_config,
      quic::QuicTransportVersion quic_version,
      quic::QuicStringPiece chlo_hash,
      const std::vector<std::string>& certs,
      const std::string& cert_sct,
      const std::string& signature,
      const quic::ProofVerifyContext* verify_context,
      std::string* error_details,
      std::unique_ptr<quic::ProofVerifyDetails>* verify_details,
      std::unique_ptr<quic::ProofVerifierCallback> callback) override;
  quic::QuicAsyncStatus VerifyCertChain(
      const std::string& hostname,
      const std::vector<std::string>& certs,
      const std::string& ocsp_response,
      const std::string& cert_sct,
      const quic::ProofVerifyContext* verify_context,
      std::string* error_details,
      std::unique_ptr<quic::ProofVerifyDetails>* verify_details,
      std::unique_ptr<quic::ProofVerifierCallback> callback) override;
  std::unique_ptr<quic::ProofVerifyContext> CreateDefaultContext() override;

 private:
  class Job;

  void OnJobComplete(Job* job);

  // Set owning pointers to active jobs.
  std::map<Job*, std::unique_ptr<Job>> active_jobs_;

  // Underlying verifier used to verify certificates.
  CertVerifier* const cert_verifier_;
  CTPolicyEnforcer* const ct_policy_enforcer_;

  TransportSecurityState* const transport_security_state_;
  CTVerifier* const cert_transparency_verifier_;

  std::set<std::string> hostnames_to_allow_unknown_roots_;

  DISALLOW_COPY_AND_ASSIGN(ProofVerifierChromium);
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_PROOF_VERIFIER_CHROMIUM_H_
