// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/trial_comparison_cert_verifier_util.h"

#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/x509_util.h"

namespace net {

namespace {

scoped_refptr<ParsedCertificate> ParsedCertificateFromBuffer(
    CRYPTO_BUFFER* cert_handle,
    CertErrors* errors) {
  return ParsedCertificate::Create(bssl::UpRef(cert_handle),
                                   x509_util::DefaultParseCertificateOptions(),
                                   errors);
}

ParsedCertificateList ParsedCertificateListFromX509Certificate(
    const X509Certificate* cert) {
  CertErrors parsing_errors;

  ParsedCertificateList certs;
  scoped_refptr<ParsedCertificate> target =
      ParsedCertificateFromBuffer(cert->cert_buffer(), &parsing_errors);
  if (!target)
    return {};
  certs.push_back(target);

  for (const auto& buf : cert->intermediate_buffers()) {
    scoped_refptr<ParsedCertificate> intermediate =
        ParsedCertificateFromBuffer(buf.get(), &parsing_errors);
    if (!intermediate)
      return {};
    certs.push_back(intermediate);
  }

  return certs;
}

// Tests whether cert has multiple EV policies, and at least one matches the
// root. This is not a complete test of EV, but just enough to give a possible
// explanation as to why the platform verifier did not validate as EV while
// builtin did. (Since only the builtin verifier correctly handles multiple
// candidate EV policies.)
bool CertHasMultipleEVPoliciesAndOneMatchesRoot(const X509Certificate* cert) {
  if (cert->intermediate_buffers().empty())
    return false;

  ParsedCertificateList certs = ParsedCertificateListFromX509Certificate(cert);
  if (certs.empty())
    return false;

  ParsedCertificate* leaf = certs.front().get();
  ParsedCertificate* root = certs.back().get();

  if (!leaf->has_policy_oids())
    return false;

  const EVRootCAMetadata* ev_metadata = EVRootCAMetadata::GetInstance();
  std::set<der::Input> candidate_oids;
  for (const der::Input& oid : leaf->policy_oids()) {
    if (ev_metadata->IsEVPolicyOIDGivenBytes(oid))
      candidate_oids.insert(oid);
  }

  if (candidate_oids.size() <= 1)
    return false;

  SHA256HashValue root_fingerprint;
  crypto::SHA256HashString(root->der_cert().AsStringPiece(),
                           root_fingerprint.data,
                           sizeof(root_fingerprint.data));

  for (const der::Input& oid : candidate_oids) {
    if (ev_metadata->HasEVPolicyOIDGivenBytes(root_fingerprint, oid))
      return true;
  }

  return false;
}

}  // namespace

// Note: This ignores the result of stapled OCSP (which is the same for both
// verifiers) and informational statuses about the certificate algorithms and
// the hashes, since they will be the same if the certificate chains are the
// same.
bool CertVerifyResultEqual(const CertVerifyResult& a,
                           const CertVerifyResult& b) {
  return std::tie(a.cert_status, a.is_issued_by_known_root) ==
             std::tie(b.cert_status, b.is_issued_by_known_root) &&
         (!!a.verified_cert == !!b.verified_cert) &&
         (!a.verified_cert ||
          a.verified_cert->EqualsIncludingChain(b.verified_cert.get()));
}

TrialComparisonResult IsSynchronouslyIgnorableDifference(
    int primary_error,
    const CertVerifyResult& primary_result,
    int trial_error,
    const CertVerifyResult& trial_result) {
  DCHECK(primary_result.verified_cert);
  DCHECK(trial_result.verified_cert);

  if (primary_error == OK &&
      primary_result.verified_cert->intermediate_buffers().empty()) {
    // Platform may support trusting a leaf certificate directly. Builtin
    // verifier does not. See https://crbug.com/814994.
    return TrialComparisonResult::kIgnoredLocallyTrustedLeaf;
  }

  const bool chains_equal = primary_result.verified_cert->EqualsIncludingChain(
      trial_result.verified_cert.get());

  if (chains_equal && (trial_result.cert_status & CERT_STATUS_IS_EV) &&
      !(primary_result.cert_status & CERT_STATUS_IS_EV) &&
      (primary_error == trial_error)) {
    // The platform CertVerifyProc impls only check a single potential EV
    // policy from the leaf.  If the leaf had multiple policies, builtin
    // verifier may verify it as EV when the platform verifier did not.
    if (CertHasMultipleEVPoliciesAndOneMatchesRoot(
            trial_result.verified_cert.get())) {
      return TrialComparisonResult::kIgnoredMultipleEVPoliciesAndOneMatchesRoot;
    }
  }
  return TrialComparisonResult::kInvalid;
}

}  // namespace net