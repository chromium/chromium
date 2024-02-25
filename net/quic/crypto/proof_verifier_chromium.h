// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_PROOF_VERIFIER_CHROMIUM_H_
#define NET_QUIC_CRYPTO_PROOF_VERIFIER_CHROMIUM_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log_with_source.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/proof_verifier.h"

namespace net {

class CertVerifier;
class SCTAuditingDelegate;
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

  // True if PKP was bypassed due to a local trust anchor.
  bool pkp_bypassed = false;

  // True if there was a certificate error which should be treated as fatal,
  // and false otherwise.
  bool is_fatal_cert_error = false;
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
  ProofVerifierChromium(
      CertVerifier* cert_verifier,
      TransportSecurityState* transport_security_state,
      SCTAuditingDelegate* sct_auditing_delegate,
      std::set<std::string> hostnames_to_allow_unknown_roots,
      const NetworkAnonymizationKey& network_anonymization_key);

  ProofVerifierChromium(const ProofVerifierChromium&) = delete;
  ProofVerifierChromium& operator=(const ProofVerifierChromium&) = delete;

  ~ProofVerifierChromium() override;

  // quic::ProofVerifier interface
  quic::QuicAsyncStatus VerifyProof(
      const std::string& hostname,
      const uint16_t port,
      const std::string& server_config,
      quic::QuicTransportVersion quic_version,
      std::string_view chlo_hash,
      const std::vector<std::string>& certs,
      const std::string& cert_sct,
      const std::string& signature,
      const quic::ProofVerifyContext* verify_context,
      std::string* error_details,
      std::unique_ptr<quic::ProofVerifyDetails>* verify_details,
      std::unique_ptr<quic::ProofVerifierCallback> callback) override;
  quic::QuicAsyncStatus VerifyCertChain(
      const std::string& hostname,
      const uint16_t port,
      const std::vector<std::string>& certs,
      const std::string& ocsp_response,
      const std::string& cert_sct,
      const quic::ProofVerifyContext* verify_context,
      std::string* error_details,
      std::unique_ptr<quic::ProofVerifyDetails>* verify_details,
      uint8_t* out_alert,
      std::unique_ptr<quic::ProofVerifierCallback> callback) override;
  std::unique_ptr<quic::ProofVerifyContext> CreateDefaultContext() override;

 private:
  class Job;

  void OnJobComplete(Job* job);

  // Set owning pointers to active jobs.
  std::map<Job*, std::unique_ptr<Job>> active_jobs_;

  // Underlying verifier used to verify certificates.
  const raw_ptr<CertVerifier> cert_verifier_;

  const raw_ptr<TransportSecurityState> transport_security_state_;

  const raw_ptr<SCTAuditingDelegate> sct_auditing_delegate_;

  std::set<std::string> hostnames_to_allow_unknown_roots_;

  const NetworkAnonymizationKey network_anonymization_key_;
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_PROOF_VERIFIER_CHROMIUM_H_
