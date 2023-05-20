// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/trial_comparison_cert_verifier_util.h"

#include "build/build_config.h"
#include "crypto/sha2.h"
#include "net/base/hash_value.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/pki/cert_errors.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/x509_util.h"

namespace net {

namespace {

std::shared_ptr<const ParsedCertificate> ParsedCertificateFromBuffer(
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
  std::shared_ptr<const ParsedCertificate> target =
      ParsedCertificateFromBuffer(cert->cert_buffer(), &parsing_errors);
  if (!target)
    return {};
  certs.push_back(std::move(target));

  for (const auto& buf : cert->intermediate_buffers()) {
    std::shared_ptr<const ParsedCertificate> intermediate =
        ParsedCertificateFromBuffer(buf.get(), &parsing_errors);
    if (!intermediate)
      return {};
    certs.push_back(std::move(intermediate));
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

  const ParsedCertificate* leaf = certs.front().get();
  const ParsedCertificate* root = certs.back().get();

  if (!leaf->has_policy_oids())
    return false;

  const EVRootCAMetadata* ev_metadata = EVRootCAMetadata::GetInstance();
  std::set<der::Input> candidate_oids;
  for (const der::Input& oid : leaf->policy_oids()) {
    if (ev_metadata->IsEVPolicyOID(oid)) {
      candidate_oids.insert(oid);
    }
  }

  if (candidate_oids.size() <= 1)
    return false;

  SHA256HashValue root_fingerprint;
  crypto::SHA256HashString(root->der_cert().AsStringView(),
                           root_fingerprint.data,
                           sizeof(root_fingerprint.data));

  for (const der::Input& oid : candidate_oids) {
    if (ev_metadata->HasEVPolicyOID(root_fingerprint, oid)) {
      return true;
    }
  }

  return false;
}

SHA256HashValue GetRootHash(const X509Certificate* cert) {
  SHA256HashValue sha256;
  if (cert->intermediate_buffers().empty()) {
    return sha256;
  }
  CRYPTO_BUFFER* root = cert->intermediate_buffers().back().get();
  return X509Certificate::CalculateFingerprint256(root);
}

const SHA256HashValue lets_encrypt_dst_x3_sha256_fingerprint = {
    {0x06, 0x87, 0x26, 0x03, 0x31, 0xA7, 0x24, 0x03, 0xD9, 0x09, 0xF1,
     0x05, 0xE6, 0x9B, 0xCF, 0x0D, 0x32, 0xE1, 0xBD, 0x24, 0x93, 0xFF,
     0xC6, 0xD9, 0x20, 0x6D, 0x11, 0xBC, 0xD6, 0x77, 0x07, 0x39}};

const SHA256HashValue lets_encrypt_isrg_x1_sha256_fingerprint = {
    {0x96, 0xBC, 0xEC, 0x06, 0x26, 0x49, 0x76, 0xF3, 0x74, 0x60, 0x77,
     0x9A, 0xCF, 0x28, 0xC5, 0xA7, 0xCF, 0xE8, 0xA3, 0xC0, 0xAA, 0xE1,
     0x1A, 0x8F, 0xFC, 0xEE, 0x05, 0xC0, 0xBD, 0xDF, 0x08, 0xC6}};

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
    const CertVerifyResult& trial_result,
    bool sha1_local_anchors_enabled) {
  DCHECK(primary_result.verified_cert);
  DCHECK(trial_result.verified_cert);

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

  // SHA-1 signatures are not supported; ignore any results with expected SHA-1
  // errors. There are however a few cases with SHA-1 signatures where we might
  // want to see the difference:
  //
  //  * local anchors enabled, and one verifier built to a SHA-1 local root but
  //     the other built to a known root.
  //  * If a verifier returned a SHA-1 signature status but did not return an
  //    error.
  if (!(sha1_local_anchors_enabled &&
        (!primary_result.is_issued_by_known_root ||
         !trial_result.is_issued_by_known_root)) &&
      (primary_result.cert_status & CERT_STATUS_SHA1_SIGNATURE_PRESENT) &&
      (trial_result.cert_status & CERT_STATUS_SHA1_SIGNATURE_PRESENT) &&
      primary_error != OK && trial_error != OK) {
    return TrialComparisonResult::kIgnoredSHA1SignaturePresent;
  }

  // Differences in chain or errors don't matter much if both
  // return AUTHORITY_INVALID.
  if ((primary_result.cert_status & CERT_STATUS_AUTHORITY_INVALID) &&
      (trial_result.cert_status & CERT_STATUS_AUTHORITY_INVALID)) {
    return TrialComparisonResult::kIgnoredBothAuthorityInvalid;
  }

  // Due to differences in path building preferences we may end up with
  // different chains in cross-signing situations. These cases are ignorable if
  // the errors are equivalent and both chains end up at a known_root.
  if (!chains_equal && (primary_error == trial_error) &&
      primary_result.is_issued_by_known_root &&
      trial_result.is_issued_by_known_root &&
      (primary_result.cert_status == trial_result.cert_status)) {
    return TrialComparisonResult::kIgnoredBothKnownRoot;
  }

  // If the primary has an error and cert_status reports that a Symantec legacy
  // cert is present, ignore the error if trial reports
  // ERR_CERT_AUTHORITY_INVALID as trial will report AUTHORITY_INVALID and short
  // circuits other checks resulting in mismatching errors and cert status.
  if (primary_error != OK && trial_error == ERR_CERT_AUTHORITY_INVALID &&
      (primary_result.cert_status & CERT_STATUS_SYMANTEC_LEGACY)) {
    return TrialComparisonResult::
        kIgnoredBuiltinAuthorityInvalidPlatformSymantec;
  }

  // There is a fairly prevalant false positive where Windows users are getting
  // errors because the chain that is built goes to Lets Encrypt's old root
  // (https://crt.sh/?id=8395) due to the windows machine having an out of date
  // auth root, whereas CCV builds to Let's Encrypt's new root
  // (https://crt.sh/?id=9314791). This manifests itself as CCV saying OK
  // whereas platform reports DATE_INVALID. If we detect this case, ignore it.
  if (primary_error == ERR_CERT_DATE_INVALID && trial_error == OK &&
      (primary_result.cert_status & CERT_STATUS_ALL_ERRORS) ==
          CERT_STATUS_DATE_INVALID) {
    SHA256HashValue primary_root_hash =
        GetRootHash(primary_result.verified_cert.get());
    SHA256HashValue trial_root_hash =
        GetRootHash(trial_result.verified_cert.get());
    if (primary_root_hash == lets_encrypt_dst_x3_sha256_fingerprint &&
        trial_root_hash == lets_encrypt_isrg_x1_sha256_fingerprint) {
      return TrialComparisonResult::kIgnoredLetsEncryptExpiredRoot;
    }
  }

#if BUILDFLAG(IS_ANDROID)
  // In the case where a cert is expired and does not have a trusted root,
  // Android prefers ERR_CERT_DATE_INVALID whereas builtin prefers
  // ERR_CERT_AUTHORITY_INVALID.
  if (primary_error == ERR_CERT_DATE_INVALID &&
      trial_error == ERR_CERT_AUTHORITY_INVALID &&
      (primary_result.cert_status & CERT_STATUS_ALL_ERRORS) ==
          CERT_STATUS_DATE_INVALID &&
      (trial_result.cert_status & CERT_STATUS_ALL_ERRORS) ==
          CERT_STATUS_AUTHORITY_INVALID) {
    return TrialComparisonResult::kIgnoredAndroidErrorDatePriority;
  }
#endif

  return TrialComparisonResult::kInvalid;
}

}  // namespace net
