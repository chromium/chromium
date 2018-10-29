// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/proof_verifier_chromium.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "crypto/signature_verifier.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/x509_util.h"
#include "net/http/transport_security_state.h"
#include "net/third_party/quic/core/crypto/crypto_protocol.h"

using base::StringPrintf;
using std::string;

namespace net {

ProofVerifyDetailsChromium::ProofVerifyDetailsChromium()
    : pkp_bypassed(false), is_fatal_cert_error(false) {}

ProofVerifyDetailsChromium::~ProofVerifyDetailsChromium() {}

ProofVerifyDetailsChromium::ProofVerifyDetailsChromium(
    const ProofVerifyDetailsChromium&) = default;

quic::ProofVerifyDetails* ProofVerifyDetailsChromium::Clone() const {
  ProofVerifyDetailsChromium* other = new ProofVerifyDetailsChromium;
  other->cert_verify_result = cert_verify_result;
  other->ct_verify_result = ct_verify_result;
  return other;
}

// A Job handles the verification of a single proof.  It is owned by the
// quic::ProofVerifier. If the verification can not complete synchronously, it
// will notify the quic::ProofVerifier upon completion.
class ProofVerifierChromium::Job {
 public:
  Job(ProofVerifierChromium* proof_verifier,
      CertVerifier* cert_verifier,
      CTPolicyEnforcer* ct_policy_enforcer,
      TransportSecurityState* transport_security_state,
      CTVerifier* cert_transparency_verifier,
      int cert_verify_flags,
      const NetLogWithSource& net_log);
  ~Job();

  // Starts the proof verification.  If |quic::QUIC_PENDING| is returned, then
  // |callback| will be invoked asynchronously when the verification completes.
  quic::QuicAsyncStatus VerifyProof(
      const std::string& hostname,
      const uint16_t port,
      const std::string& server_config,
      quic::QuicTransportVersion quic_version,
      quic::QuicStringPiece chlo_hash,
      const std::vector<std::string>& certs,
      const std::string& cert_sct,
      const std::string& signature,
      std::string* error_details,
      std::unique_ptr<quic::ProofVerifyDetails>* verify_details,
      std::unique_ptr<quic::ProofVerifierCallback> callback);

  // Starts the certificate chain verification of |certs|.  If
  // |quic::QUIC_PENDING| is returned, then |callback| will be invoked
  // asynchronously when the verification completes.
  quic::QuicAsyncStatus VerifyCertChain(
      const std::string& hostname,
      const std::vector<std::string>& certs,
      std::string* error_details,
      std::unique_ptr<quic::ProofVerifyDetails>* verify_details,
      std::unique_ptr<quic::ProofVerifierCallback> callback);

 private:
  enum State {
    STATE_NONE,
    STATE_VERIFY_CERT,
    STATE_VERIFY_CERT_COMPLETE,
  };

  // Convert |certs| to |cert_|(X509Certificate). Returns true if successful.
  bool GetX509Certificate(
      const std::vector<string>& certs,
      std::string* error_details,
      std::unique_ptr<quic::ProofVerifyDetails>* verify_details);

  // Start the cert verification.
  quic::QuicAsyncStatus VerifyCert(
      const string& hostname,
      const uint16_t port,
      std::string* error_details,
      std::unique_ptr<quic::ProofVerifyDetails>* verify_details,
      std::unique_ptr<quic::ProofVerifierCallback> callback);

  int DoLoop(int last_io_result);
  void OnIOComplete(int result);
  int DoVerifyCert(int result);
  int DoVerifyCertComplete(int result);

  bool VerifySignature(const std::string& signed_data,
                       quic::QuicTransportVersion quic_version,
                       quic::QuicStringPiece chlo_hash,
                       const std::string& signature,
                       const std::string& cert);

  // Proof verifier to notify when this jobs completes.
  ProofVerifierChromium* proof_verifier_;

  // The underlying verifier used for verifying certificates.
  CertVerifier* verifier_;
  std::unique_ptr<CertVerifier::Request> cert_verifier_request_;

  CTPolicyEnforcer* policy_enforcer_;

  TransportSecurityState* transport_security_state_;

  CTVerifier* cert_transparency_verifier_;

  // |hostname| specifies the hostname for which |certs| is a valid chain.
  std::string hostname_;
  // |port| specifies the target port for the connection.
  uint16_t port_;

  std::unique_ptr<quic::ProofVerifierCallback> callback_;
  std::unique_ptr<ProofVerifyDetailsChromium> verify_details_;
  std::string error_details_;

  // X509Certificate from a chain of DER encoded certificates.
  scoped_refptr<X509Certificate> cert_;

  // |cert_verify_flags| is bitwise OR'd of CertVerifier::VerifyFlags and it is
  // passed to CertVerifier::Verify.
  int cert_verify_flags_;

  // If set to true, enforces policy checking in DoVerifyCertComplete().
  bool enforce_policy_checking_;

  State next_state_;

  base::TimeTicks start_time_;

  NetLogWithSource net_log_;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

ProofVerifierChromium::Job::Job(
    ProofVerifierChromium* proof_verifier,
    CertVerifier* cert_verifier,
    CTPolicyEnforcer* ct_policy_enforcer,
    TransportSecurityState* transport_security_state,
    CTVerifier* cert_transparency_verifier,
    int cert_verify_flags,
    const NetLogWithSource& net_log)
    : proof_verifier_(proof_verifier),
      verifier_(cert_verifier),
      policy_enforcer_(ct_policy_enforcer),
      transport_security_state_(transport_security_state),
      cert_transparency_verifier_(cert_transparency_verifier),
      cert_verify_flags_(cert_verify_flags),
      enforce_policy_checking_(true),
      next_state_(STATE_NONE),
      start_time_(base::TimeTicks::Now()),
      net_log_(net_log) {
  CHECK(proof_verifier_);
  CHECK(verifier_);
  CHECK(policy_enforcer_);
  CHECK(transport_security_state_);
  CHECK(cert_transparency_verifier_);
}

ProofVerifierChromium::Job::~Job() {
  base::TimeTicks end_time = base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES("Net.QuicSession.VerifyProofTime",
                      end_time - start_time_);
  // |hostname_| will always be canonicalized to lowercase.
  if (hostname_.compare("www.google.com") == 0) {
    UMA_HISTOGRAM_TIMES("Net.QuicSession.VerifyProofTime.google",
                        end_time - start_time_);
  }
}

quic::QuicAsyncStatus ProofVerifierChromium::Job::VerifyProof(
    const string& hostname,
    const uint16_t port,
    const string& server_config,
    quic::QuicTransportVersion quic_version,
    quic::QuicStringPiece chlo_hash,
    const std::vector<string>& certs,
    const std::string& cert_sct,
    const string& signature,
    std::string* error_details,
    std::unique_ptr<quic::ProofVerifyDetails>* verify_details,
    std::unique_ptr<quic::ProofVerifierCallback> callback) {
  DCHECK(error_details);
  DCHECK(verify_details);
  DCHECK(callback);

  error_details->clear();

  if (STATE_NONE != next_state_) {
    *error_details = "Certificate is already set and VerifyProof has begun";
    DLOG(DFATAL) << *error_details;
    return quic::QUIC_FAILURE;
  }

  verify_details_.reset(new ProofVerifyDetailsChromium);

  // Converts |certs| to |cert_|.
  if (!GetX509Certificate(certs, error_details, verify_details))
    return quic::QUIC_FAILURE;

  // Note that this is a completely synchronous operation: The CT Log Verifier
  // gets all the data it needs for SCT verification and does not do any
  // external communication.
  cert_transparency_verifier_->Verify(
      hostname, cert_.get(), std::string(), cert_sct,
      &verify_details_->ct_verify_result.scts, net_log_);

  // We call VerifySignature first to avoid copying of server_config and
  // signature.
  if (!signature.empty() && !VerifySignature(server_config, quic_version,
                                             chlo_hash, signature, certs[0])) {
    *error_details = "Failed to verify signature of server config";
    DLOG(WARNING) << *error_details;
    verify_details_->cert_verify_result.cert_status = CERT_STATUS_INVALID;
    *verify_details = std::move(verify_details_);
    return quic::QUIC_FAILURE;
  }

  DCHECK(enforce_policy_checking_);
  return VerifyCert(hostname, port, error_details, verify_details,
                    std::move(callback));
}

quic::QuicAsyncStatus ProofVerifierChromium::Job::VerifyCertChain(
    const string& hostname,
    const std::vector<string>& certs,
    std::string* error_details,
    std::unique_ptr<quic::ProofVerifyDetails>* verify_details,
    std::unique_ptr<quic::ProofVerifierCallback> callback) {
  DCHECK(error_details);
  DCHECK(verify_details);
  DCHECK(callback);

  error_details->clear();

  if (STATE_NONE != next_state_) {
    *error_details = "Certificate is already set and VerifyCertChain has begun";
    DLOG(DFATAL) << *error_details;
    return quic::QUIC_FAILURE;
  }

  verify_details_.reset(new ProofVerifyDetailsChromium);

  // Converts |certs| to |cert_|.
  if (!GetX509Certificate(certs, error_details, verify_details))
    return quic::QUIC_FAILURE;

  enforce_policy_checking_ = false;
  // |port| is not needed because |enforce_policy_checking_| is false.
  return VerifyCert(hostname, /*port=*/0, error_details, verify_details,
                    std::move(callback));
}

bool ProofVerifierChromium::Job::GetX509Certificate(
    const std::vector<string>& certs,
    std::string* error_details,
    std::unique_ptr<quic::ProofVerifyDetails>* verify_details) {
  if (certs.empty()) {
    *error_details = "Failed to create certificate chain. Certs are empty.";
    DLOG(WARNING) << *error_details;
    verify_details_->cert_verify_result.cert_status = CERT_STATUS_INVALID;
    *verify_details = std::move(verify_details_);
    return false;
  }

  // Convert certs to X509Certificate.
  std::vector<quic::QuicStringPiece> cert_pieces(certs.size());
  for (unsigned i = 0; i < certs.size(); i++) {
    cert_pieces[i] = quic::QuicStringPiece(certs[i]);
  }
  cert_ = X509Certificate::CreateFromDERCertChain(cert_pieces);
  if (!cert_.get()) {
    *error_details = "Failed to create certificate chain";
    DLOG(WARNING) << *error_details;
    verify_details_->cert_verify_result.cert_status = CERT_STATUS_INVALID;
    *verify_details = std::move(verify_details_);
    return false;
  }
  return true;
}

quic::QuicAsyncStatus ProofVerifierChromium::Job::VerifyCert(
    const string& hostname,
    const uint16_t port,
    std::string* error_details,
    std::unique_ptr<quic::ProofVerifyDetails>* verify_details,
    std::unique_ptr<quic::ProofVerifierCallback> callback) {
  hostname_ = hostname;
  port_ = port;

  next_state_ = STATE_VERIFY_CERT;
  switch (DoLoop(OK)) {
    case OK:
      *verify_details = std::move(verify_details_);
      return quic::QUIC_SUCCESS;
    case ERR_IO_PENDING:
      callback_ = std::move(callback);
      return quic::QUIC_PENDING;
    default:
      *error_details = error_details_;
      *verify_details = std::move(verify_details_);
      return quic::QUIC_FAILURE;
  }
}

int ProofVerifierChromium::Job::DoLoop(int last_result) {
  int rv = last_result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_VERIFY_CERT:
        DCHECK(rv == OK);
        rv = DoVerifyCert(rv);
        break;
      case STATE_VERIFY_CERT_COMPLETE:
        rv = DoVerifyCertComplete(rv);
        break;
      case STATE_NONE:
      default:
        rv = ERR_UNEXPECTED;
        LOG(DFATAL) << "unexpected state " << state;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
  return rv;
}

void ProofVerifierChromium::Job::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING) {
    std::unique_ptr<quic::ProofVerifierCallback> callback(std::move(callback_));
    // Callback expects quic::ProofVerifyDetails not ProofVerifyDetailsChromium.
    std::unique_ptr<quic::ProofVerifyDetails> verify_details(
        std::move(verify_details_));
    callback->Run(rv == OK, error_details_, &verify_details);
    // Will delete |this|.
    proof_verifier_->OnJobComplete(this);
  }
}

int ProofVerifierChromium::Job::DoVerifyCert(int result) {
  next_state_ = STATE_VERIFY_CERT_COMPLETE;

  return verifier_->Verify(
      CertVerifier::RequestParams(cert_, hostname_, cert_verify_flags_,
                                  std::string()),
      &verify_details_->cert_verify_result,
      base::Bind(&ProofVerifierChromium::Job::OnIOComplete,
                 base::Unretained(this)),
      &cert_verifier_request_, net_log_);
}

int ProofVerifierChromium::Job::DoVerifyCertComplete(int result) {
  base::UmaHistogramSparse("Net.QuicSession.CertVerificationResult", -result);
  cert_verifier_request_.reset();

  const CertVerifyResult& cert_verify_result =
      verify_details_->cert_verify_result;
  const CertStatus cert_status = cert_verify_result.cert_status;

  // If the connection was good, check HPKP and CT status simultaneously,
  // but prefer to treat the HPKP error as more serious, if there was one.
  if (enforce_policy_checking_ &&
      (result == OK ||
       (IsCertificateError(result) && IsCertStatusMinorError(cert_status)))) {
    ct::SCTList verified_scts = ct::SCTsMatchingStatus(
        verify_details_->ct_verify_result.scts, ct::SCT_STATUS_OK);

    verify_details_->ct_verify_result.policy_compliance =
        policy_enforcer_->CheckCompliance(
            cert_verify_result.verified_cert.get(), verified_scts, net_log_);
    if (verify_details_->cert_verify_result.cert_status & CERT_STATUS_IS_EV) {
      if (verify_details_->ct_verify_result.policy_compliance !=
              ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS &&
          verify_details_->ct_verify_result.policy_compliance !=
              ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY) {
        verify_details_->cert_verify_result.cert_status |=
            CERT_STATUS_CT_COMPLIANCE_FAILED;
        verify_details_->cert_verify_result.cert_status &= ~CERT_STATUS_IS_EV;
      }

      // Record the CT compliance status for connections with EV certificates,
      // to distinguish how often EV status is being dropped due to failing CT
      // compliance.
      if (verify_details_->cert_verify_result.is_issued_by_known_root) {
        UMA_HISTOGRAM_ENUMERATION(
            "Net.CertificateTransparency.EVCompliance2.QUIC",
            verify_details_->ct_verify_result.policy_compliance,
            ct::CTPolicyCompliance::CT_POLICY_COUNT);
      }
    }

    // Record the CT compliance of every connection to get an overall picture of
    // how many connections are CT-compliant.
    if (verify_details_->cert_verify_result.is_issued_by_known_root) {
      UMA_HISTOGRAM_ENUMERATION(
          "Net.CertificateTransparency.ConnectionComplianceStatus2.QUIC",
          verify_details_->ct_verify_result.policy_compliance,
          ct::CTPolicyCompliance::CT_POLICY_COUNT);
    }

    int ct_result = OK;
    TransportSecurityState::CTRequirementsStatus ct_requirement_status =
        transport_security_state_->CheckCTRequirements(
            HostPortPair(hostname_, port_),
            cert_verify_result.is_issued_by_known_root,
            cert_verify_result.public_key_hashes,
            cert_verify_result.verified_cert.get(), cert_.get(),
            verify_details_->ct_verify_result.scts,
            TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
            verify_details_->ct_verify_result.policy_compliance);
    if (ct_requirement_status != TransportSecurityState::CT_NOT_REQUIRED) {
      verify_details_->ct_verify_result.policy_compliance_required = true;
      if (verify_details_->cert_verify_result.is_issued_by_known_root) {
        // Record the CT compliance of connections for which compliance is
        // required; this helps answer the question: "Of all connections that
        // are supposed to be serving valid CT information, how many fail to do
        // so?"
        UMA_HISTOGRAM_ENUMERATION(
            "Net.CertificateTransparency.CTRequiredConnectionComplianceStatus2."
            "QUIC",
            verify_details_->ct_verify_result.policy_compliance,
            ct::CTPolicyCompliance::CT_POLICY_COUNT);
      }
    } else {
      verify_details_->ct_verify_result.policy_compliance_required = false;
    }

    switch (ct_requirement_status) {
      case TransportSecurityState::CT_REQUIREMENTS_NOT_MET:
        verify_details_->cert_verify_result.cert_status |=
            CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED;
        ct_result = ERR_CERTIFICATE_TRANSPARENCY_REQUIRED;
        break;
      case TransportSecurityState::CT_REQUIREMENTS_MET:
      case TransportSecurityState::CT_NOT_REQUIRED:
        // Intentional fallthrough; this case is just here to make sure that all
        // possible values of CheckCTRequirements() are handled.
        break;
    }

    TransportSecurityState::PKPStatus pin_validity =
        transport_security_state_->CheckPublicKeyPins(
            HostPortPair(hostname_, port_),
            cert_verify_result.is_issued_by_known_root,
            cert_verify_result.public_key_hashes, cert_.get(),
            cert_verify_result.verified_cert.get(),
            TransportSecurityState::ENABLE_PIN_REPORTS,
            &verify_details_->pinning_failure_log);
    switch (pin_validity) {
      case TransportSecurityState::PKPStatus::VIOLATED:
        result = ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN;
        verify_details_->cert_verify_result.cert_status |=
            CERT_STATUS_PINNED_KEY_MISSING;
        break;
      case TransportSecurityState::PKPStatus::BYPASSED:
        verify_details_->pkp_bypassed = true;
        FALLTHROUGH;
      case TransportSecurityState::PKPStatus::OK:
        // Do nothing.
        break;
    }
    if (result != ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN && ct_result != OK)
      result = ct_result;
  }

  verify_details_->is_fatal_cert_error =
      IsCertStatusError(cert_status) && !IsCertStatusMinorError(cert_status) &&
      transport_security_state_->ShouldSSLErrorsBeFatal(hostname_);

  if (result != OK) {
    std::string error_string = ErrorToString(result);
    error_details_ = StringPrintf("Failed to verify certificate chain: %s",
                                  error_string.c_str());
    DLOG(WARNING) << error_details_;
  }

  // Exit DoLoop and return the result to the caller to VerifyProof.
  DCHECK_EQ(STATE_NONE, next_state_);
  return result;
}

bool ProofVerifierChromium::Job::VerifySignature(
    const string& signed_data,
    quic::QuicTransportVersion quic_version,
    quic::QuicStringPiece chlo_hash,
    const string& signature,
    const string& cert) {
  size_t size_bits;
  X509Certificate::PublicKeyType type;
  X509Certificate::GetPublicKeyInfo(cert_->cert_buffer(), &size_bits, &type);
  crypto::SignatureVerifier::SignatureAlgorithm algorithm;
  switch (type) {
    case X509Certificate::kPublicKeyTypeRSA:
      algorithm = crypto::SignatureVerifier::RSA_PSS_SHA256;
      break;
    case X509Certificate::kPublicKeyTypeECDSA:
      algorithm = crypto::SignatureVerifier::ECDSA_SHA256;
      break;
    default:
      LOG(ERROR) << "Unsupported public key type " << type;
      return false;
  }

  crypto::SignatureVerifier verifier;
  if (!x509_util::SignatureVerifierInitWithCertificate(
          &verifier, algorithm, base::as_bytes(base::make_span(signature)),
          cert_->cert_buffer())) {
    DLOG(WARNING) << "SignatureVerifierInitWithCertificate failed";
    return false;
  }

  verifier.VerifyUpdate(
      base::as_bytes(base::make_span(quic::kProofSignatureLabel)));
  uint32_t len = chlo_hash.length();
  verifier.VerifyUpdate(base::as_bytes(base::make_span(&len, 1)));
  verifier.VerifyUpdate(base::as_bytes(base::make_span(chlo_hash)));
  verifier.VerifyUpdate(base::as_bytes(base::make_span(signed_data)));

  if (!verifier.VerifyFinal()) {
    DLOG(WARNING) << "VerifyFinal failed";
    return false;
  }

  DVLOG(1) << "VerifyFinal success";
  return true;
}

ProofVerifierChromium::ProofVerifierChromium(
    CertVerifier* cert_verifier,
    CTPolicyEnforcer* ct_policy_enforcer,
    TransportSecurityState* transport_security_state,
    CTVerifier* cert_transparency_verifier)
    : cert_verifier_(cert_verifier),
      ct_policy_enforcer_(ct_policy_enforcer),
      transport_security_state_(transport_security_state),
      cert_transparency_verifier_(cert_transparency_verifier) {
  DCHECK(cert_verifier_);
  DCHECK(ct_policy_enforcer_);
  DCHECK(transport_security_state_);
  DCHECK(cert_transparency_verifier_);
}

ProofVerifierChromium::~ProofVerifierChromium() {}

quic::QuicAsyncStatus ProofVerifierChromium::VerifyProof(
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
    std::unique_ptr<quic::ProofVerifierCallback> callback) {
  if (!verify_context) {
    DLOG(FATAL) << "Missing proof verify context";
    *error_details = "Missing context";
    return quic::QUIC_FAILURE;
  }
  const ProofVerifyContextChromium* chromium_context =
      reinterpret_cast<const ProofVerifyContextChromium*>(verify_context);
  std::unique_ptr<Job> job = std::make_unique<Job>(
      this, cert_verifier_, ct_policy_enforcer_, transport_security_state_,
      cert_transparency_verifier_, chromium_context->cert_verify_flags,
      chromium_context->net_log);
  quic::QuicAsyncStatus status = job->VerifyProof(
      hostname, port, server_config, quic_version, chlo_hash, certs, cert_sct,
      signature, error_details, verify_details, std::move(callback));
  if (status == quic::QUIC_PENDING) {
    Job* job_ptr = job.get();
    active_jobs_[job_ptr] = std::move(job);
  }
  return status;
}

quic::QuicAsyncStatus ProofVerifierChromium::VerifyCertChain(
    const std::string& hostname,
    const std::vector<std::string>& certs,
    const quic::ProofVerifyContext* verify_context,
    std::string* error_details,
    std::unique_ptr<quic::ProofVerifyDetails>* verify_details,
    std::unique_ptr<quic::ProofVerifierCallback> callback) {
  if (!verify_context) {
    *error_details = "Missing context";
    return quic::QUIC_FAILURE;
  }
  const ProofVerifyContextChromium* chromium_context =
      reinterpret_cast<const ProofVerifyContextChromium*>(verify_context);
  std::unique_ptr<Job> job = std::make_unique<Job>(
      this, cert_verifier_, ct_policy_enforcer_, transport_security_state_,
      cert_transparency_verifier_, chromium_context->cert_verify_flags,
      chromium_context->net_log);
  quic::QuicAsyncStatus status = job->VerifyCertChain(
      hostname, certs, error_details, verify_details, std::move(callback));
  if (status == quic::QUIC_PENDING) {
    Job* job_ptr = job.get();
    active_jobs_[job_ptr] = std::move(job);
  }
  return status;
}

std::unique_ptr<quic::ProofVerifyContext>
ProofVerifierChromium::CreateDefaultContext() {
  return std::make_unique<ProofVerifyContextChromium>(0,
                                                      net::NetLogWithSource());
}

void ProofVerifierChromium::OnJobComplete(Job* job) {
  active_jobs_.erase(job);
}

}  // namespace net
