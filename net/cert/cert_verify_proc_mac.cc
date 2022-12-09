// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_mac.h"

#include <CommonCrypto/CommonDigest.h>
#include <CoreServices/CoreServices.h>
#include <Security/Security.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "crypto/mac_security_services_lock.h"
#include "crypto/sha2.h"
#include "net/base/hash_value.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/known_roots.h"
#include "net/cert/known_roots_mac.h"
#include "net/cert/pki/certificate_policies.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_apple.h"
#include "net/cert/x509_util_mac.h"

// CSSM functions are deprecated as of OSX 10.7, but have no replacement.
// https://bugs.chromium.org/p/chromium/issues/detail?id=590914#c1
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

using base::ScopedCFTypeRef;

namespace net {

namespace {

const void* kResultDebugDataKey = &kResultDebugDataKey;

typedef OSStatus (*SecTrustCopyExtendedResultFuncPtr)(SecTrustRef,
                                                      CFDictionaryRef*);
using CertEvidenceInfo = CertVerifyProcMac::ResultDebugData::CertEvidenceInfo;

int NetErrorFromOSStatus(OSStatus status) {
  switch (status) {
    case noErr:
      return OK;
    case errSecNotAvailable:
    case errSecNoCertificateModule:
    case errSecNoPolicyModule:
      return ERR_NOT_IMPLEMENTED;
    case errSecAuthFailed:
      return ERR_ACCESS_DENIED;
    default: {
      OSSTATUS_LOG(ERROR, status) << "Unknown error mapped to ERR_FAILED";
      return ERR_FAILED;
    }
  }
}

// Beginning with macOS 10.13, certificate verification is dispatched
// to trustd, which uses OSStatus internally to track errors, and
// then maps the internal codes into CSSM codes for applications still
// calling the deprecated (since 10.7) APIs.
//
// The mapping is maintained in SecPolicyChecks.list, to see the
// checks applied to leaves/intermediates/roots/chains and what
// failure of those checks will cause, both the OSStatus and the
// mapped CSSM error code.
//
// Not all checks in the table are applicable; some only apply to
// Apple-specific services (e.g. iTunes checking for an Apple
// policy), so only those applicable to TLS are mapped here.
//
// The downside is that it does mean that as Apple introduces
// additional checks (e.g. as done in 10.15), any failures of these
// checks are initially mapped to ERR_CERT_INVALID for safety, even
// if there may be a more applicable CertStatus code.
CertStatus CertStatusFromOSStatus(OSStatus status) {
  switch (status) {
    case noErr:
      return 0;

    case CSSMERR_APPLETP_HOSTNAME_MISMATCH:
      return CERT_STATUS_COMMON_NAME_INVALID;

    case CSSMERR_TP_CERT_EXPIRED:
    case CSSMERR_TP_CERT_NOT_VALID_YET:
      return CERT_STATUS_DATE_INVALID;

    case CSSMERR_APPLETP_TRUST_SETTING_DENY:
    case CSSMERR_TP_NOT_TRUSTED:
    // CSSMERR_TP_VERIFY_ACTION_FAILED is used when CT is required
    // and not present. The OS rejected this chain, and so mapping
    // to CERT_STATUS_CT_COMPLIANCE_FAILED (which is informational,
    // as policy enforcement is not handled in the CertVerifier)
    // would cause this error to be ignored and mapped to
    // CERT_STATUS_INVALID. Rather than do that, mark it simply as
    // "untrusted". The CT_COMPLIANCE_FAILED bit is not set, since
    // it's not necessarily a compliance failure with the embedder's
    // CT policy. It's a bit of a hack, but hopefully temporary.
    // TP_NOT_TRUSTED is somewhat similar. It applies for
    // situations where a root isn't trusted or an intermediate
    // isn't trusted, when a key is restricted, or when the calling
    // application requested CT enforcement (which CertVerifier
    // should never being doing).
    case CSSMERR_TP_VERIFY_ACTION_FAILED:
      return CERT_STATUS_AUTHORITY_INVALID;

    case CSSMERR_APPLETP_INVALID_AUTHORITY_ID:
    case CSSMERR_APPLETP_INVALID_CA:
    case CSSMERR_APPLETP_INVALID_EMPTY_SUBJECT:
    case CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE:
    case CSSMERR_APPLETP_INVALID_KEY_USAGE:
    case CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION:
    case CSSMERR_APPLETP_NO_BASIC_CONSTRAINTS:
    case CSSMERR_APPLETP_PATH_LEN_CONSTRAINT:
    case CSSMERR_APPLETP_UNKNOWN_CERT_EXTEN:
    case CSSMERR_APPLETP_UNKNOWN_CRITICAL_EXTEN:
    case CSSMERR_CSP_ALGID_MISMATCH:
    // INVALID_POLICY_IDENTIFIERS and INVALID_NAME are used for
    // certificates that violate the constraints imposed upon the
    // issuer. Nominally this could be mapped to
    // CERT_STATUS_AUTHORITY_INVALID, except the trustd behaviour
    // is to treat this as a fatal (non-recoverable) error. That
    // behavior is preserved here for consistency with Safari.
    case CSSMERR_TP_INVALID_POLICY_IDENTIFIERS:
    case CSSMERR_TP_INVALID_NAME:
      return CERT_STATUS_INVALID;

    // In trustd, an unsupported algorithm is CSP_ALGID_MISMATCH,
    // which should cause a path building failure, while supported
    // but weak algorithms use this code.
    case CSSMERR_CSP_INVALID_DIGEST_ALGORITHM:
      return CERT_STATUS_WEAK_SIGNATURE_ALGORITHM;

    // In trustd, certificates that are too weak to process, period,
    // are mapped to INVALID_CERTIFICATE. However, certificates which
    // are too weak according to compliance policies (e.g. restrictions
    // for publicly trusted certificates) are mapped to UNSUPPORTED_KEY_SIZE.
    case CSSMERR_CSP_UNSUPPORTED_KEY_SIZE:
      return CERT_STATUS_WEAK_KEY;

    case CSSMERR_TP_CERT_REVOKED:
      return CERT_STATUS_REVOKED;

    case CSSMERR_APPLETP_INCOMPLETE_REVOCATION_CHECK:
      return CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;

    // In the trustd world, if a CRL suspends a certificate,
    // that's signaled by TP_CERT_REVOKED, with the revocation
    // reason available in the error details dictionary. The
    // SUSPENDED error is repurposed to indicate failure to
    // comply with the macOS 10.15+ limits on certificate
    // lifetimes - https://support.apple.com/en-us/HT210176
    case CSSMERR_TP_CERT_SUSPENDED:
      return CERT_STATUS_VALIDITY_TOO_LONG;

    // CSSMERR_TP_INVALID_CERTIFICATE is unfortunate. It may be
    // used to signal a weak key (CERT_STATUS_WEAK_KEY), which
    // would be accompanied by a kSecTrustResultFatalTrustFailure, while
    // the other situations (such as an invalid certificate, a
    // name constraint violation, or a policy constraint violation)
    // would be accompanied by a kSecTrustResultRecoverableTrustFailure.
    // However, CertVerifier treats these as inverted: name constraint or
    // policy violations are fatal (CERT_STATUS_INVALID), while WEAK_KEY
    // may be recoverable.
    // Further, because macOS attempts to gather all the errors, a different
    // fatal error may have occurred elsewhere in the chain, so the overall
    // result can't be used to distinguish individual certificate errors.
    // For this complicated reason, the weak key case is mapped to
    // CERT_STATUS_INVALID for safety, rather than mapping the policy
    // violations as weak keys.
    case CSSMERR_TP_INVALID_CERTIFICATE:
      return CERT_STATUS_INVALID;

    default: {
      // Failure was due to something Chromium doesn't define a
      // specific status for (such as basic constraints violation, or
      // unknown critical extension)
      OSSTATUS_LOG(WARNING, status)
          << "Unknown error mapped to CERT_STATUS_INVALID";
      return CERT_STATUS_INVALID;
    }
  }
}

// Creates a series of SecPolicyRefs to be added to a SecTrustRef used to
// validate a certificate for an SSL server. |flags| is a bitwise-OR of
// VerifyFlags that can further alter how trust is validated, such as how
// revocation is checked. If successful, returns noErr, and stores the
// resultant array of SecPolicyRefs in |policies|.
OSStatus CreateTrustPolicies(int flags, ScopedCFTypeRef<CFArrayRef>* policies) {
  ScopedCFTypeRef<CFMutableArrayRef> local_policies(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  if (!local_policies)
    return memFullErr;

  ScopedCFTypeRef<SecPolicyRef> ssl_policy(
      SecPolicyCreateSSL(/*server=*/true, /*hostname=*/nullptr));
  if (!ssl_policy)
    return errSecNoPolicyModule;
  CFArrayAppendValue(local_policies, ssl_policy);

  // Explicitly add revocation policies, in order to override system
  // revocation checking policies and instead respect the application-level
  // revocation preference.
  if (flags & CertVerifyProc::VERIFY_REV_CHECKING_ENABLED) {
    // If revocation checking is requested, enable checking and require positive
    // results. Note that this will fail if there are certs with no
    // CRLDistributionPoints or OCSP AIA urls, which differs from the behavior
    // of |enable_revocation_checking| on pre-10.12. There does not appear to be
    // a way around this, but it shouldn't matter much in practice since
    // revocation checking is generally used with EV certs, where it is expected
    // that all certs include revocation mechanisms.
    ScopedCFTypeRef<SecPolicyRef> revocation_policy(
        SecPolicyCreateRevocation(kSecRevocationUseAnyAvailableMethod |
                                  kSecRevocationRequirePositiveResponse));
    if (!revocation_policy)
      return errSecNoPolicyModule;
    CFArrayAppendValue(local_policies, revocation_policy);
  }

  policies->reset(local_policies.release());
  return noErr;
}

// Stores the constructed certificate chain |cert_chain| into
// |*verify_result|. |cert_chain| must not be empty.
void CopyCertChainToVerifyResult(CFArrayRef cert_chain,
                                 CertVerifyResult* verify_result) {
  DCHECK_LT(0, CFArrayGetCount(cert_chain));

  base::ScopedCFTypeRef<SecCertificateRef> verified_cert;
  std::vector<base::ScopedCFTypeRef<SecCertificateRef>> verified_chain;
  for (CFIndex i = 0, count = CFArrayGetCount(cert_chain); i < count; ++i) {
    SecCertificateRef chain_cert = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(cert_chain, i)));
    if (i == 0) {
      verified_cert.reset(chain_cert, base::scoped_policy::RETAIN);
    } else {
      verified_chain.emplace_back(chain_cert, base::scoped_policy::RETAIN);
    }
  }
  if (!verified_cert) {
    NOTREACHED();
    verify_result->cert_status |= CERT_STATUS_INVALID;
    return;
  }

  scoped_refptr<X509Certificate> verified_cert_with_chain =
      x509_util::CreateX509CertificateFromSecCertificate(verified_cert,
                                                         verified_chain);
  if (verified_cert_with_chain)
    verify_result->verified_cert = std::move(verified_cert_with_chain);
  else
    verify_result->cert_status |= CERT_STATUS_INVALID;
}

// Returns true if the certificate uses MD2, MD4, MD5, or SHA1, and false
// otherwise. A return of false also includes the case where the signature
// algorithm couldn't be conclusively labeled as weak.
bool CertUsesWeakHash(SecCertificateRef cert_handle) {
  x509_util::CSSMCachedCertificate cached_cert;
  OSStatus status = cached_cert.Init(cert_handle);
  if (status)
    return false;

  x509_util::CSSMFieldValue signature_field;
  status =
      cached_cert.GetField(&CSSMOID_X509V1SignatureAlgorithm, &signature_field);
  if (status || !signature_field.field())
    return false;

  const CSSM_X509_ALGORITHM_IDENTIFIER* sig_algorithm =
      signature_field.GetAs<CSSM_X509_ALGORITHM_IDENTIFIER>();
  if (!sig_algorithm)
    return false;

  const CSSM_OID* alg_oid = &sig_algorithm->algorithm;

  return (x509_util::CSSMOIDEqual(alg_oid, &CSSMOID_MD2WithRSA) ||
          x509_util::CSSMOIDEqual(alg_oid, &CSSMOID_MD4WithRSA) ||
          x509_util::CSSMOIDEqual(alg_oid, &CSSMOID_MD5WithRSA) ||
          x509_util::CSSMOIDEqual(alg_oid, &CSSMOID_SHA1WithRSA) ||
          x509_util::CSSMOIDEqual(alg_oid, &CSSMOID_SHA1WithRSA_OIW) ||
          x509_util::CSSMOIDEqual(alg_oid, &CSSMOID_SHA1WithDSA) ||
          x509_util::CSSMOIDEqual(alg_oid, &CSSMOID_SHA1WithDSA_CMS) ||
          x509_util::CSSMOIDEqual(alg_oid, &CSSMOID_SHA1WithDSA_JDK) ||
          x509_util::CSSMOIDEqual(alg_oid, &CSSMOID_ECDSA_WithSHA1));
}

// Returns true if the intermediates (excluding trusted certificates) use a
// weak hashing algorithm, but the target does not use a weak hash.
bool IsWeakChainBasedOnHashingAlgorithms(
    CFArrayRef cert_chain,
    const std::vector<CertEvidenceInfo>& chain_info) {
  DCHECK_LT(0, CFArrayGetCount(cert_chain));
  DCHECK_EQ(chain_info.size(),
            static_cast<size_t>(CFArrayGetCount(cert_chain)));

  bool intermediates_contain_weak_hash = false;
  bool leaf_uses_weak_hash = false;

  for (CFIndex i = 0, count = CFArrayGetCount(cert_chain); i < count; ++i) {
    SecCertificateRef chain_cert = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(cert_chain, i)));

    if ((chain_info[i].status_bits & CSSM_CERT_STATUS_IS_IN_ANCHORS) ||
        (chain_info[i].status_bits & CSSM_CERT_STATUS_IS_ROOT)) {
      // The current certificate is either in the user's trusted store or is
      // a root (self-signed) certificate. Ignore the signature algorithm for
      // these certificates, as it is meaningless for security. We allow
      // self-signed certificates (i == 0 & IS_ROOT), since we accept that
      // any security assertions by such a cert are inherently meaningless.
      continue;
    }

    if (CertUsesWeakHash(chain_cert)) {
      if (i == 0) {
        leaf_uses_weak_hash = true;
      } else {
        intermediates_contain_weak_hash = true;
      }
    }
  }

  return !leaf_uses_weak_hash && intermediates_contain_weak_hash;
}

// Checks if |*cert| has a Certificate Policies extension containing either
// of |ev_policy_oid| or anyPolicy.
bool HasPolicyOrAnyPolicy(const ParsedCertificate* cert,
                          const der::Input& ev_policy_oid) {
  if (!cert->has_policy_oids())
    return false;

  for (const der::Input& policy_oid : cert->policy_oids()) {
    if (policy_oid == ev_policy_oid || policy_oid == der::Input(kAnyPolicyOid))
      return true;
  }
  return false;
}

// Looks for known EV policy OIDs in |cert_input|, if one is found it will be
// stored in |*ev_policy_oid| as a DER-encoded OID value (no tag or length).
void GetCandidateEVPolicy(const X509Certificate* cert_input,
                          std::string* ev_policy_oid) {
  ev_policy_oid->clear();

  std::shared_ptr<const ParsedCertificate> cert(ParsedCertificate::Create(
      bssl::UpRef(cert_input->cert_buffer()), {}, nullptr));
  if (!cert)
    return;

  if (!cert->has_policy_oids())
    return;

  EVRootCAMetadata* metadata = EVRootCAMetadata::GetInstance();
  for (const der::Input& policy_oid : cert->policy_oids()) {
    if (metadata->IsEVPolicyOID(policy_oid)) {
      *ev_policy_oid = policy_oid.AsString();

      // De-prioritize the CA/Browser forum Extended Validation policy
      // (2.23.140.1.1). See crbug.com/705285.
      if (!EVRootCAMetadata::IsCaBrowserForumEvOid(policy_oid))
        break;
    }
  }
}

// Checks that the certificate chain of |cert| has policies consistent with
// |ev_policy_oid_string|. The leaf is not checked, as it is assumed that is
// where the policy came from.
bool CheckCertChainEV(const X509Certificate* cert,
                      const std::string& ev_policy_oid_string) {
  der::Input ev_policy_oid(&ev_policy_oid_string);
  const std::vector<bssl::UniquePtr<CRYPTO_BUFFER>>& cert_chain =
      cert->intermediate_buffers();

  // Root should have matching policy in EVRootCAMetadata.
  if (cert_chain.empty())
    return false;
  SHA256HashValue fingerprint =
      X509Certificate::CalculateFingerprint256(cert_chain.back().get());
  EVRootCAMetadata* metadata = EVRootCAMetadata::GetInstance();
  if (!metadata->HasEVPolicyOID(fingerprint, ev_policy_oid))
    return false;

  // Intermediates should have Certificate Policies extension with the EV policy
  // or AnyPolicy.
  for (size_t i = 0; i < cert_chain.size() - 1; ++i) {
    std::shared_ptr<const ParsedCertificate> intermediate_cert(
        ParsedCertificate::Create(bssl::UpRef(cert_chain[i].get()), {},
                                  nullptr));
    if (!intermediate_cert)
      return false;
    if (!HasPolicyOrAnyPolicy(intermediate_cert.get(), ev_policy_oid))
      return false;
  }

  return true;
}

void AppendPublicKeyHashesAndUpdateKnownRoot(CFArrayRef chain,
                                             HashValueVector* hashes,
                                             bool* known_root) {
  // Walk the chain in reverse, to optimize for IsKnownRoot checks.
  for (CFIndex i = CFArrayGetCount(chain); i > 0; i--) {
    SecCertificateRef cert = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(chain, i - 1)));

    CSSM_DATA cert_data;
    OSStatus err = SecCertificateGetData(cert, &cert_data);
    DCHECK_EQ(err, noErr);
    base::StringPiece der_bytes(reinterpret_cast<const char*>(cert_data.Data),
                               cert_data.Length);
    base::StringPiece spki_bytes;
    if (!asn1::ExtractSPKIFromDERCert(der_bytes, &spki_bytes))
      continue;

    HashValue sha256(HASH_VALUE_SHA256);
    CC_SHA256(spki_bytes.data(), spki_bytes.size(), sha256.data());
    hashes->push_back(sha256);

    if (!*known_root) {
      *known_root =
          GetNetTrustAnchorHistogramIdForSPKI(sha256) != 0 || IsKnownRoot(cert);
    }
  }
  // Reverse the hash array, to maintain the leaf-first ordering.
  std::reverse(hashes->begin(), hashes->end());
}

enum CRLSetResult {
  kCRLSetOk,
  kCRLSetRevoked,
  kCRLSetUnknown,
};

// CheckRevocationWithCRLSet attempts to check each element of |cert_list|
// against |crl_set|. It returns:
//   kCRLSetRevoked: if any element of the chain is known to have been revoked.
//   kCRLSetUnknown: if there is no fresh information about the leaf
//       certificate in the chain or if the CRLSet has expired.
//
//       Only the leaf certificate is considered for coverage because some
//       intermediates have CRLs with no revocations (after filtering) and
//       those CRLs are pruned from the CRLSet at generation time. This means
//       that some EV sites would otherwise take the hit of an OCSP lookup for
//       no reason.
//   kCRLSetOk: otherwise.
CRLSetResult CheckRevocationWithCRLSet(CFArrayRef chain, CRLSet* crl_set) {
  if (CFArrayGetCount(chain) == 0)
    return kCRLSetOk;

  // error is set to true if any errors are found. It causes such chains to be
  // considered as not covered.
  bool error = false;
  // last_covered is set to the coverage state of the previous certificate. The
  // certificates are iterated over backwards thus, after the iteration,
  // |last_covered| contains the coverage state of the leaf certificate.
  bool last_covered = false;

  // We iterate from the root certificate down to the leaf, keeping track of
  // the issuer's SPKI at each step.
  std::string issuer_spki_hash;
  for (CFIndex i = CFArrayGetCount(chain); i > 0; i--) {
    SecCertificateRef cert = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(chain, i - 1)));

    CSSM_DATA cert_data;
    OSStatus err = SecCertificateGetData(cert, &cert_data);
    if (err != noErr) {
      NOTREACHED();
      error = true;
      continue;
    }
    base::StringPiece der_bytes(reinterpret_cast<const char*>(cert_data.Data),
                                cert_data.Length);
    base::StringPiece spki, subject;
    if (!asn1::ExtractSPKIFromDERCert(der_bytes, &spki) ||
        !asn1::ExtractSubjectFromDERCert(der_bytes, &subject)) {
      NOTREACHED();
      error = true;
      continue;
    }

    const std::string spki_hash = crypto::SHA256HashString(spki);
    x509_util::CSSMCachedCertificate cached_cert;
    if (cached_cert.Init(cert) != CSSM_OK) {
      NOTREACHED();
      error = true;
      continue;
    }
    x509_util::CSSMFieldValue serial_number;
    err = cached_cert.GetField(&CSSMOID_X509V1SerialNumber, &serial_number);
    if (err || !serial_number.field()) {
      NOTREACHED();
      error = true;
      continue;
    }

    base::StringPiece serial(
        reinterpret_cast<const char*>(serial_number.field()->Data),
        serial_number.field()->Length);

    CRLSet::Result result = crl_set->CheckSPKI(spki_hash);

    if (result != CRLSet::REVOKED)
      result = crl_set->CheckSubject(subject, spki_hash);
    if (result != CRLSet::REVOKED && !issuer_spki_hash.empty())
      result = crl_set->CheckSerial(serial, issuer_spki_hash);

    issuer_spki_hash = spki_hash;

    switch (result) {
      case CRLSet::REVOKED:
        return kCRLSetRevoked;
      case CRLSet::UNKNOWN:
        last_covered = false;
        continue;
      case CRLSet::GOOD:
        last_covered = true;
        continue;
      default:
        NOTREACHED();
        error = true;
        continue;
    }
  }

  if (error || !last_covered || crl_set->IsExpired())
    return kCRLSetUnknown;
  return kCRLSetOk;
}

// Builds and evaluates a SecTrustRef for the certificate chain contained
// in |cert_array|, using the verification policies in |trust_policies|. On
// success, returns OK, and updates |trust_ref|, |trust_result|,
// |verified_chain|, and |chain_info| with the verification results. On
// failure, no output parameters are modified.
//
// Note: An OK return does not mean that |cert_array| is trusted, merely that
// verification was performed successfully.
//
// This function should only be called while the Mac Security Services lock is
// held.
int BuildAndEvaluateSecTrustRef(CFArrayRef cert_array,
                                CFArrayRef trust_policies,
                                CFDataRef ocsp_response_ref,
                                CFArrayRef sct_array_ref,
                                int flags,
                                CFArrayRef keychain_search_list,
                                ScopedCFTypeRef<SecTrustRef>* trust_ref,
                                SecTrustResultType* trust_result,
                                ScopedCFTypeRef<CFArrayRef>* verified_chain,
                                std::vector<CertEvidenceInfo>* chain_info) {
  SecTrustRef tmp_trust = nullptr;
  OSStatus status = SecTrustCreateWithCertificates(cert_array, trust_policies,
                                                   &tmp_trust);
  if (status)
    return NetErrorFromOSStatus(status);
  ScopedCFTypeRef<SecTrustRef> scoped_tmp_trust(tmp_trust);

  if (TestRootCerts::HasInstance()) {
    status = TestRootCerts::GetInstance()->FixupSecTrustRef(tmp_trust);
    if (status)
      return NetErrorFromOSStatus(status);
  }

  if (keychain_search_list) {
    status = SecTrustSetKeychains(tmp_trust, keychain_search_list);
    if (status)
      return NetErrorFromOSStatus(status);
  }

  if (ocsp_response_ref) {
    status = SecTrustSetOCSPResponse(tmp_trust, ocsp_response_ref);
    if (status)
      return NetErrorFromOSStatus(status);
  }

  if (sct_array_ref) {
    if (__builtin_available(macOS 10.14.2, *)) {
      status = SecTrustSetSignedCertificateTimestamps(tmp_trust, sct_array_ref);
      if (status)
        return NetErrorFromOSStatus(status);
    }
  }

  CSSM_APPLE_TP_ACTION_DATA tp_action_data;
  memset(&tp_action_data, 0, sizeof(tp_action_data));
  tp_action_data.Version = CSSM_APPLE_TP_ACTION_VERSION;
  // Allow CSSM to download any missing intermediate certificates if an
  // authorityInfoAccess extension or issuerAltName extension is present.
  tp_action_data.ActionFlags = CSSM_TP_ACTION_FETCH_CERT_FROM_NET |
                               CSSM_TP_ACTION_TRUST_SETTINGS;

  // Note: For EV certificates, the Apple TP will handle setting these flags
  // as part of EV evaluation.
  if (flags & CertVerifyProc::VERIFY_REV_CHECKING_ENABLED) {
    // Require a positive result from an OCSP responder or a CRL (or both)
    // for every certificate in the chain. The Apple TP automatically
    // excludes the self-signed root from this requirement. If a certificate
    // is missing both a crlDistributionPoints extension and an
    // authorityInfoAccess extension with an OCSP responder URL, then we
    // will get a kSecTrustResultRecoverableTrustFailure back from
    // SecTrustEvaluate(), with a
    // CSSMERR_APPLETP_INCOMPLETE_REVOCATION_CHECK error code. In that case,
    // we'll set our own result to include
    // CERT_STATUS_NO_REVOCATION_MECHANISM. If one or both extensions are
    // present, and a check fails (server unavailable, OCSP retry later,
    // signature mismatch), then we'll set our own result to include
    // CERT_STATUS_UNABLE_TO_CHECK_REVOCATION.
    tp_action_data.ActionFlags |= CSSM_TP_ACTION_REQUIRE_REV_PER_CERT;

    // Note, even if revocation checking is disabled, SecTrustEvaluate() will
    // modify the OCSP options so as to attempt OCSP checking if it believes a
    // certificate may chain to an EV root. However, because network fetches
    // are disabled in CreateTrustPolicies() when revocation checking is
    // disabled, these will only go against the local cache.
  }

  ScopedCFTypeRef<CFDataRef> action_data_ref(CFDataCreate(
      kCFAllocatorDefault, reinterpret_cast<UInt8*>(&tp_action_data),
      sizeof(tp_action_data)));
  if (!action_data_ref)
    return ERR_OUT_OF_MEMORY;
  status = SecTrustSetParameters(tmp_trust, CSSM_TP_ACTION_DEFAULT,
                                 action_data_ref.get());
  if (status)
    return NetErrorFromOSStatus(status);

  // Verify the certificate. A non-zero result from SecTrustGetResult()
  // indicates that some fatal error occurred and the chain couldn't be
  // processed, not that the chain contains no errors. We need to examine the
  // output of SecTrustGetResult() to determine that.
  SecTrustResultType tmp_trust_result;
  status = SecTrustEvaluate(tmp_trust, &tmp_trust_result);
  if (status)
    return NetErrorFromOSStatus(status);
  CFArrayRef tmp_verified_chain = nullptr;
  CSSM_TP_APPLE_EVIDENCE_INFO* tmp_chain_info;
  status = SecTrustGetResult(tmp_trust, &tmp_trust_result, &tmp_verified_chain,
                             &tmp_chain_info);
  if (status)
    return NetErrorFromOSStatus(status);

  // WARNING: Beginning with OS X 10.13, |tmp_chain_info| may be freed by any
  // other accesses via SecTrust APIs to |tmp_trust|, so copy the data.
  chain_info->clear();
  for (CFIndex i = 0, chain_length = CFArrayGetCount(tmp_verified_chain);
       i < chain_length; ++i) {
    CertEvidenceInfo info;
    info.status_bits = tmp_chain_info[i].StatusBits;
    info.status_codes.assign(
        tmp_chain_info[i].StatusCodes,
        tmp_chain_info[i].StatusCodes + tmp_chain_info[i].NumStatusCodes);
    chain_info->push_back(std::move(info));
  }

  trust_ref->swap(scoped_tmp_trust);
  *trust_result = tmp_trust_result;
  verified_chain->reset(tmp_verified_chain);

  return OK;
}

// Runs path building & verification loop for |cert|, given |flags|. This is
// split into a separate function so verification can be repeated with different
// flags. This function does not handle EV.
int VerifyWithGivenFlags(X509Certificate* cert,
                         const std::string& hostname,
                         const std::string& ocsp_response,
                         const std::string& sct_list,
                         const int flags,
                         bool rev_checking_soft_fail,
                         CRLSet* crl_set,
                         CertVerifyResult* verify_result,
                         CRLSetResult* completed_chain_crl_result) {
  ScopedCFTypeRef<CFArrayRef> trust_policies;
  OSStatus status = CreateTrustPolicies(flags, &trust_policies);
  if (status)
    return NetErrorFromOSStatus(status);

  *completed_chain_crl_result = kCRLSetUnknown;

  ScopedCFTypeRef<CFDataRef> ocsp_response_ref;
  if (!ocsp_response.empty()) {
    ocsp_response_ref.reset(
        CFDataCreate(kCFAllocatorDefault,
                     reinterpret_cast<const UInt8*>(ocsp_response.data()),
                     base::checked_cast<CFIndex>(ocsp_response.size())));
    if (!ocsp_response_ref)
      return ERR_OUT_OF_MEMORY;
  }

  ScopedCFTypeRef<CFMutableArrayRef> sct_array_ref;
  if (!sct_list.empty()) {
    if (__builtin_available(macOS 10.14.2, *)) {
      std::vector<base::StringPiece> decoded_sct_list;
      if (ct::DecodeSCTList(sct_list, &decoded_sct_list)) {
        sct_array_ref.reset(CFArrayCreateMutable(kCFAllocatorDefault,
                                                 decoded_sct_list.size(),
                                                 &kCFTypeArrayCallBacks));
        if (!sct_array_ref)
          return ERR_OUT_OF_MEMORY;
        for (const auto& sct : decoded_sct_list) {
          ScopedCFTypeRef<CFDataRef> sct_ref(CFDataCreate(
              kCFAllocatorDefault, reinterpret_cast<const UInt8*>(sct.data()),
              base::checked_cast<CFIndex>(sct.size())));
          if (!sct_ref)
            return ERR_OUT_OF_MEMORY;
          CFArrayAppendValue(sct_array_ref.get(), sct_ref.get());
        }
      }
    }
  }

  // Serialize all calls that may use the Keychain, to work around various
  // issues in OS X 10.6+ with multi-threaded access to Security.framework.
  base::AutoLock lock(crypto::GetMacSecurityServicesLock());

  ScopedCFTypeRef<SecTrustRef> trust_ref;
  SecTrustResultType trust_result = kSecTrustResultDeny;
  ScopedCFTypeRef<CFArrayRef> completed_chain;
  std::vector<CertEvidenceInfo> chain_info;
  bool candidate_untrusted = true;
  bool candidate_weak = false;

  // OS X lacks proper path discovery; it will take the input certs and never
  // backtrack the graph attempting to discover valid paths.
  // This can create issues in some situations:
  // - When OS X changes the trust store, there may be a chain
  //     A -> B -> C -> D
  //   where OS X trusts D (on some versions) and trusts C (on some versions).
  //   If a server supplies a chain A, B, C (cross-signed by D), then this chain
  //   will successfully validate on systems that trust D, but fail for systems
  //   that trust C. If the server supplies a chain of A -> B, then it forces
  //   all clients to fetch C (via AIA) if they trust D, and not all clients
  //   (notably, Firefox and Android) will do this, thus breaking them.
  //   An example of this is the Verizon Business Services root - GTE CyberTrust
  //   and Baltimore CyberTrust roots represent old and new roots that cause
  //   issues depending on which version of OS X being used.
  //
  // - A server may be (misconfigured) to send an expired intermediate
  //   certificate. On platforms with path discovery, the graph traversal
  //   will back up to immediately before this intermediate, and then
  //   attempt an AIA fetch or retrieval from local store. However, OS X
  //   does not do this, and thus prevents access. While this is ostensibly
  //   a server misconfiguration issue, the fact that it works on other
  //   platforms is a jarring inconsistency for users.
  //
  // - When OS X trusts both C and D (simultaneously), it's possible that the
  //   version of C signed by D is signed using a weak algorithm (e.g. SHA-1),
  //   while the version of C in the trust store's signature doesn't matter.
  //   Since a 'strong' chain exists, it would be desirable to prefer this
  //   chain.
  //
  // - A variant of the above example, it may be that the version of B sent by
  //   the server is signed using a weak algorithm, but the version of B
  //   present in the AIA of A is signed using a strong algorithm. Since a
  //   'strong' chain exists, it would be desirable to prefer this chain.
  //
  // - A user keychain may contain a less desirable intermediate or root.
  //   OS X gives the user keychains higher priority than the system keychain,
  //   so it may build a weak chain.
  //
  // Because of this, the code below first attempts to validate the peer's
  // identity using the supplied chain. If it is not trusted (e.g. the OS only
  // trusts C, but the version of C signed by D was sent, and D is not trusted),
  // or if it contains a weak chain, it will begin lopping off certificates
  // from the end of the chain and attempting to verify. If a stronger, trusted
  // chain is found, it is used, otherwise, the algorithm continues until only
  // the peer's certificate remains.
  //
  // If the loop does not find a trusted chain, the loop will be repeated with
  // the keychain search order altered to give priority to the System Roots
  // keychain.
  //
  // This does cause a performance hit for these users, but only in cases where
  // OS X is building weaker chains than desired, or when it would otherwise
  // fail the connection.
  for (bool try_reordered_keychain : {false, true}) {
    ScopedCFTypeRef<CFArrayRef> scoped_alternate_keychain_search_list;
    if (try_reordered_keychain) {
      CFArrayRef keychain_search_list;
      status = SecKeychainCopySearchList(&keychain_search_list);
      if (status)
        return NetErrorFromOSStatus(status);
      scoped_alternate_keychain_search_list.reset(keychain_search_list);

      CFMutableArrayRef mutable_keychain_search_list = CFArrayCreateMutableCopy(
          kCFAllocatorDefault,
          CFArrayGetCount(scoped_alternate_keychain_search_list.get()) + 1,
          scoped_alternate_keychain_search_list.get());
      if (!mutable_keychain_search_list)
        return ERR_OUT_OF_MEMORY;
      scoped_alternate_keychain_search_list.reset(mutable_keychain_search_list);

      SecKeychainRef keychain;
      // Get a reference to the System Roots keychain. The System Roots
      // keychain is not normally present in the keychain search list, but is
      // implicitly checked after the keychains in the search list. By
      // including it directly, force it to be checked first.  This is a gross
      // hack, but the path is known to be valid through macOS 12.
      status = SecKeychainOpen(
          "/System/Library/Keychains/SystemRootCertificates.keychain",
          &keychain);
      if (status)
        return NetErrorFromOSStatus(status);
      ScopedCFTypeRef<SecKeychainRef> scoped_keychain(keychain);

      CFArrayInsertValueAtIndex(mutable_keychain_search_list, 0, keychain);
    }

    ScopedCFTypeRef<CFMutableArrayRef> cert_array(
        x509_util::CreateSecCertificateArrayForX509Certificate(
            cert, x509_util::InvalidIntermediateBehavior::kIgnore));
    if (!cert_array) {
      verify_result->cert_status |= CERT_STATUS_INVALID;
      return ERR_CERT_INVALID;
    }

    // Beginning with the certificate chain as supplied by the server, attempt
    // to verify the chain. If a failure is encountered, trim a certificate
    // from the end (so long as one remains) and retry, in the hope of forcing
    // OS X to find a better path.
    while (CFArrayGetCount(cert_array) > 0) {
      ScopedCFTypeRef<SecTrustRef> temp_ref;
      SecTrustResultType temp_trust_result = kSecTrustResultDeny;
      ScopedCFTypeRef<CFArrayRef> temp_chain;
      std::vector<CertEvidenceInfo> temp_chain_info;

      int rv = BuildAndEvaluateSecTrustRef(
          cert_array, trust_policies, ocsp_response_ref.get(),
          sct_array_ref.get(), flags,
          scoped_alternate_keychain_search_list.get(), &temp_ref,
          &temp_trust_result, &temp_chain, &temp_chain_info);
      if (rv != OK)
        return rv;

      // Check to see if the path |temp_chain| has been revoked. This is less
      // than ideal to perform after path building, rather than during, because
      // there may be multiple paths to trust anchors, and only some of them
      // are revoked. Ideally, CRLSets would be part of path building, which
      // they are when using NSS (Linux) or CryptoAPI (Windows).
      //
      // The CRLSet checking is performed inside the loop in the hope that if a
      // path is revoked, it's an older path, and the only reason it was built
      // is because the server forced it (by supplying an older or less
      // desirable intermediate) or because the user had installed a
      // certificate in their Keychain forcing this path. However, this means
      // its still possible for a CRLSet block of an intermediate to prevent
      // access, even when there is a 'good' chain. To fully remedy this, a
      // solution might be to have CRLSets contain enough knowledge about what
      // the 'desired' path might be, but for the time being, the
      // implementation is kept as 'simple' as it can be.
      CRLSetResult crl_result = CheckRevocationWithCRLSet(temp_chain, crl_set);
      bool untrusted = (temp_trust_result != kSecTrustResultUnspecified &&
                        temp_trust_result != kSecTrustResultProceed) ||
                       crl_result == kCRLSetRevoked;
      bool weak_chain = false;
      if (CFArrayGetCount(temp_chain) == 0) {
        // If the chain is empty, it cannot be trusted or have recoverable
        // errors.
        DCHECK(untrusted);
        DCHECK_NE(kSecTrustResultRecoverableTrustFailure, temp_trust_result);
      } else {
        weak_chain =
            IsWeakChainBasedOnHashingAlgorithms(temp_chain, temp_chain_info);
      }
      // Set the result to the current chain if:
      // - This is the first verification attempt. This ensures that if
      //   everything is awful (e.g. it may just be an untrusted cert), that
      //   what is reported is exactly what was sent by the server
      // - If the current chain is trusted, and the old chain was not trusted,
      //   then prefer this chain. This ensures that if there is at least a
      //   valid path to a trust anchor, it's preferred over reporting an error.
      // - If the current chain is trusted, and the old chain is trusted, but
      //   the old chain contained weak algorithms while the current chain only
      //   contains strong algorithms, then prefer the current chain over the
      //   old chain.
      //
      // Note: If the leaf certificate itself is weak, then the only
      // consideration is whether or not there is a trusted chain. That's
      // because no amount of path discovery will fix a weak leaf.
      if (!trust_ref || (!untrusted && (candidate_untrusted ||
                                        (candidate_weak && !weak_chain)))) {
        trust_ref = temp_ref;
        trust_result = temp_trust_result;
        completed_chain = temp_chain;
        *completed_chain_crl_result = crl_result;
        chain_info = std::move(temp_chain_info);

        candidate_untrusted = untrusted;
        candidate_weak = weak_chain;
      }
      // Short-circuit when a current, trusted chain is found.
      if (!untrusted && !weak_chain)
        break;
      // Trim a cert off the end of chain, but if the chain is longer that 10
      // certs, trim to at most 10 certs.
      constexpr int kMaxTrimmedChainLength = 10;
      if (CFArrayGetCount(cert_array) > kMaxTrimmedChainLength) {
        CFArrayReplaceValues(
            cert_array,
            CFRangeMake(kMaxTrimmedChainLength,
                        CFArrayGetCount(cert_array) - kMaxTrimmedChainLength),
            /*newValues=*/nullptr, /*newCount=*/0);
      } else {
        CFArrayRemoveValueAtIndex(cert_array, CFArrayGetCount(cert_array) - 1);
      }
    }
    // Short-circuit when a current, trusted chain is found.
    if (!candidate_untrusted && !candidate_weak)
      break;
  }

  if (flags & CertVerifyProc::VERIFY_REV_CHECKING_ENABLED)
    verify_result->cert_status |= CERT_STATUS_REV_CHECKING_ENABLED;

  if (*completed_chain_crl_result == kCRLSetRevoked)
    verify_result->cert_status |= CERT_STATUS_REVOKED;

  if (CFArrayGetCount(completed_chain) > 0) {
    CopyCertChainToVerifyResult(completed_chain, verify_result);
  }

  // As of macOS 10.13, if |trust_result| (from SecTrustGetResult) returns
  // kSecTrustResultInvalid, subsequent invocations of SecTrust APIs may
  // result in revalidating the SecTrust. In releases earlier than 10.13, this
  // call would have additional information, except that information is unused
  // and irrelevant if the result was invalid, so the placeholder
  // errSecInternalError is fine.
  OSStatus cssm_result = errSecInternalError;
  if (trust_result != kSecTrustResultInvalid) {
    status = SecTrustGetCssmResultCode(trust_ref, &cssm_result);
    if (status)
      return NetErrorFromOSStatus(status);
  }

  // Evaluate the results
  switch (trust_result) {
    case kSecTrustResultUnspecified:
    case kSecTrustResultProceed:
      // Certificate chain is valid and trusted ("unspecified" indicates that
      // the user has not explicitly set a trust setting)
      break;

    // According to SecTrust.h, kSecTrustResultConfirm isn't returned on 10.5+,
    // and it is marked deprecated in the 10.9 SDK.
    case kSecTrustResultDeny:
      // Certificate chain is explicitly untrusted.
      verify_result->cert_status |= CERT_STATUS_AUTHORITY_INVALID;
      break;

    case kSecTrustResultFatalTrustFailure:
      // Certificate chain has a failure that cannot be overridden by the user.
    case kSecTrustResultRecoverableTrustFailure:
      // Certificate chain has a failure that can be overridden by the user.

      // Prior to 10.13, a violation of key size restrictions would, at minimum,
      // result in a TP_VERIFY_ACTION_FAILED error. In 10.13+, this error has
      // different semantics, and weak keys can no longer be distinguished
      // as such.
      verify_result->cert_status |= CertStatusFromOSStatus(cssm_result);

      // Walk the chain of error codes in the CSSM_TP_APPLE_EVIDENCE_INFO
      // structure which can catch multiple errors from each certificate.
      for (CFIndex index = 0, chain_count = CFArrayGetCount(completed_chain);
           index < chain_count; ++index) {
        if (chain_info[index].status_bits & CSSM_CERT_STATUS_EXPIRED ||
            chain_info[index].status_bits & CSSM_CERT_STATUS_NOT_VALID_YET)
          verify_result->cert_status |= CERT_STATUS_DATE_INVALID;
        if (!IsCertStatusError(verify_result->cert_status) &&
            chain_info[index].status_codes.empty()) {
          LOG(WARNING) << "chain_info[" << index
                       << "].status_codes is empty, chain_info[" << index
                       << "].status_bits is " << chain_info[index].status_bits;
        }
        for (int32_t status_code : chain_info[index].status_codes) {
          verify_result->cert_status |= CertStatusFromOSStatus(status_code);
        }
      }

      if (!IsCertStatusError(verify_result->cert_status)) {
        LOG(ERROR) << "cssm_result=" << cssm_result;
        verify_result->cert_status |= CERT_STATUS_INVALID;
        NOTREACHED();
      }
      break;

    default:
      verify_result->cert_status |= CertStatusFromOSStatus(cssm_result);
      if (!IsCertStatusError(verify_result->cert_status)) {
        LOG(WARNING) << "trust_result=" << trust_result;
        verify_result->cert_status |= CERT_STATUS_INVALID;
      }
      break;
  }

  // Hostname validation is handled by CertVerifyProc, so mask off any errors
  // that SecTrustEvaluate may have set, as its results are not used.
  verify_result->cert_status &= ~CERT_STATUS_COMMON_NAME_INVALID;

  if (rev_checking_soft_fail) {
    verify_result->cert_status &= ~(CERT_STATUS_NO_REVOCATION_MECHANISM |
                                    CERT_STATUS_UNABLE_TO_CHECK_REVOCATION);
  }

  AppendPublicKeyHashesAndUpdateKnownRoot(
      completed_chain, &verify_result->public_key_hashes,
      &verify_result->is_issued_by_known_root);

  CertVerifyProcMac::ResultDebugData::Create(
      trust_result, cssm_result, std::move(chain_info), verify_result);

  if (IsCertStatusError(verify_result->cert_status))
    return MapCertStatusToNetError(verify_result->cert_status);

  return OK;
}

}  // namespace

CertVerifyProcMac::ResultDebugData::CertEvidenceInfo::CertEvidenceInfo() =
    default;
CertVerifyProcMac::ResultDebugData::CertEvidenceInfo::~CertEvidenceInfo() =
    default;
CertVerifyProcMac::ResultDebugData::CertEvidenceInfo::CertEvidenceInfo(
    const CertEvidenceInfo&) = default;
CertVerifyProcMac::ResultDebugData::CertEvidenceInfo::CertEvidenceInfo(
    CertEvidenceInfo&&) = default;

CertVerifyProcMac::ResultDebugData::ResultDebugData(
    uint32_t trust_result,
    int32_t result_code,
    std::vector<CertEvidenceInfo> status_chain)
    : trust_result_(trust_result),
      result_code_(result_code),
      status_chain_(std::move(status_chain)) {}

CertVerifyProcMac::ResultDebugData::~ResultDebugData() = default;

CertVerifyProcMac::ResultDebugData::ResultDebugData(const ResultDebugData&) =
    default;

// static
const CertVerifyProcMac::ResultDebugData*
CertVerifyProcMac::ResultDebugData::Get(
    const base::SupportsUserData* debug_data) {
  return static_cast<ResultDebugData*>(
      debug_data->GetUserData(kResultDebugDataKey));
}

// static
void CertVerifyProcMac::ResultDebugData::Create(
    uint32_t trust_result,
    int32_t result_code,
    std::vector<CertEvidenceInfo> status_chain,
    base::SupportsUserData* debug_data) {
  debug_data->SetUserData(kResultDebugDataKey,
                          std::make_unique<ResultDebugData>(
                              trust_result, result_code, status_chain));
}

std::unique_ptr<base::SupportsUserData::Data>
CertVerifyProcMac::ResultDebugData::Clone() {
  return std::make_unique<ResultDebugData>(*this);
}

CertVerifyProcMac::CertVerifyProcMac() = default;

CertVerifyProcMac::~CertVerifyProcMac() = default;

bool CertVerifyProcMac::SupportsAdditionalTrustAnchors() const {
  return false;
}

int CertVerifyProcMac::VerifyInternal(
    X509Certificate* cert,
    const std::string& hostname,
    const std::string& ocsp_response,
    const std::string& sct_list,
    int flags,
    CRLSet* crl_set,
    const CertificateList& additional_trust_anchors,
    CertVerifyResult* verify_result,
    const NetLogWithSource& net_log) {
  // Save the input state of |*verify_result|, which may be needed to re-do
  // verification with different flags.
  const CertVerifyResult input_verify_result(*verify_result);

  // Check for EV policy in leaf cert.
  std::string candidate_ev_policy_oid;
  GetCandidateEVPolicy(cert, &candidate_ev_policy_oid);

  CRLSetResult completed_chain_crl_result;
  int rv = VerifyWithGivenFlags(cert, hostname, ocsp_response, sct_list, flags,
                                /*rev_checking_soft_fail=*/true, crl_set,
                                verify_result, &completed_chain_crl_result);
  if (rv != OK)
    return rv;

  if (!candidate_ev_policy_oid.empty() &&
      CheckCertChainEV(verify_result->verified_cert.get(),
                       candidate_ev_policy_oid)) {
    // EV policies check out and the verification succeeded. Revocation checking
    // may have been done, but revocation checking is not required for EV certs
    // (see https://crbug.com/705285).
    verify_result->cert_status |= CERT_STATUS_IS_EV;
  }

  LogNameNormalizationMetrics(".Mac", verify_result->verified_cert.get(),
                              verify_result->is_issued_by_known_root);

  return OK;
}

}  // namespace net

#pragma clang diagnostic pop  // "-Wdeprecated-declarations"
