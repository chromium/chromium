// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_ios.h"

#include <CommonCrypto/CommonDigest.h>

#include <string_view>

#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/known_roots.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_apple.h"

using base::apple::ScopedCFTypeRef;

namespace net {

namespace {

int NetErrorFromOSStatus(OSStatus status) {
  switch (status) {
    case noErr:
      return OK;
    case errSecNotAvailable:
      return ERR_NOT_IMPLEMENTED;
    case errSecAuthFailed:
      return ERR_ACCESS_DENIED;
    default:
      return ERR_FAILED;
  }
}

// Maps errors from OSStatus codes to CertStatus flags.
//
// The selection of errors is based off of Apple's SecPolicyChecks.list, and
// any unknown errors are mapped to CERT_STATUS_INVALID for safety.
CertStatus CertStatusFromOSStatus(OSStatus status) {
  switch (status) {
    case errSecHostNameMismatch:
      return CERT_STATUS_COMMON_NAME_INVALID;

    case errSecCertificateExpired:
    case errSecCertificateNotValidYet:
      return CERT_STATUS_DATE_INVALID;

    case errSecCreateChainFailed:
    case errSecNotTrusted:
    // errSecVerifyActionFailed is used when CT is required
    // and not present. The OS rejected this chain, and so mapping
    // to CERT_STATUS_CT_COMPLIANCE_FAILED (which is informational,
    // as policy enforcement is not handled in the CertVerifier)
    // would cause this error to be ignored and mapped to
    // CERT_STATUS_INVALID. Rather than do that, mark it simply as
    // "untrusted". The CT_COMPLIANCE_FAILED bit is not set, since
    // it's not necessarily a compliance failure with the embedder's
    // CT policy. It's a bit of a hack, but hopefully temporary.
    // errSecNotTrusted is somewhat similar. It applies for
    // situations where a root isn't trusted or an intermediate
    // isn't trusted, when a key is restricted, or when the calling
    // application requested CT enforcement (which CertVerifier
    // should never being doing).
    case errSecVerifyActionFailed:
      return CERT_STATUS_AUTHORITY_INVALID;

    case errSecInvalidIDLinkage:
    case errSecNoBasicConstraintsCA:
    case errSecInvalidSubjectName:
    case errSecInvalidExtendedKeyUsage:
    case errSecInvalidKeyUsageForPolicy:
    case errSecMissingRequiredExtension:
    case errSecNoBasicConstraints:
    case errSecPathLengthConstraintExceeded:
    case errSecUnknownCertExtension:
    case errSecUnknownCriticalExtensionFlag:
    // errSecCertificatePolicyNotAllowed and errSecCertificateNameNotAllowed
    // are used for certificates that violate the constraints imposed upon the
    // issuer. Nominally this could be mapped to CERT_STATUS_AUTHORITY_INVALID,
    // except the trustd behaviour is to treat this as a fatal
    // (non-recoverable) error. That behavior is preserved here for consistency
    // with Safari.
    case errSecCertificatePolicyNotAllowed:
    case errSecCertificateNameNotAllowed:
      return CERT_STATUS_INVALID;

    // Unfortunately, iOS's handling of weak digest algorithms and key sizes
    // doesn't map exactly to Chrome's. errSecInvalidDigestAlgorithm and
    // errSecUnsupportedKeySize may indicate errors that iOS considers fatal
    // (too weak to process at all) or recoverable (too weak according to
    // compliance policies).
    // Further, because SecTrustEvaluateWithError only returns a single error
    // code, a fatal error may have occurred elsewhere in the chain, so the
    // overall result can't be used to distinguish individual certificate
    // errors. For this complicated reason, the weak key and weak digest cases
    // also map to CERT_STATUS_INVALID for safety.
    case errSecInvalidDigestAlgorithm:
      return CERT_STATUS_WEAK_SIGNATURE_ALGORITHM | CERT_STATUS_INVALID;
    case errSecUnsupportedKeySize:
      return CERT_STATUS_WEAK_KEY | CERT_STATUS_INVALID;

    case errSecCertificateRevoked:
      return CERT_STATUS_REVOKED;

    case errSecIncompleteCertRevocationCheck:
      return CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;

    case errSecCertificateValidityPeriodTooLong:
      return CERT_STATUS_VALIDITY_TOO_LONG;

    case errSecInvalidCertificateRef:
    case errSecInvalidName:
    case errSecInvalidPolicyIdentifiers:
      return CERT_STATUS_INVALID;

    // This function should only be called on errors, so should always return a
    // CertStatus code that is considered an error. If the input is unexpectedly
    // errSecSuccess, return CERT_STATUS_INVALID for safety.
    case errSecSuccess:
    default:
      OSSTATUS_LOG(WARNING, status)
          << "Unknown error mapped to CERT_STATUS_INVALID";
      return CERT_STATUS_INVALID;
  }
}

// Creates a series of SecPolicyRefs to be added to a SecTrustRef used to
// validate a certificate for an SSL server. |hostname| contains the name of
// the SSL server that the certificate should be verified against. If
// successful, returns noErr, and stores the resultant array of SecPolicyRefs
// in |policies|.
OSStatus CreateTrustPolicies(ScopedCFTypeRef<CFArrayRef>* policies) {
  ScopedCFTypeRef<CFMutableArrayRef> local_policies(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  if (!local_policies)
    return errSecAllocate;

  base::apple::ScopedCFTypeRef<SecPolicyRef> ssl_policy(
      SecPolicyCreateBasicX509());
  CFArrayAppendValue(local_policies.get(), ssl_policy.get());
  ssl_policy.reset(SecPolicyCreateSSL(/*server=*/true, /*hostname=*/nullptr));
  CFArrayAppendValue(local_policies.get(), ssl_policy.get());

  *policies = std::move(local_policies);
  return noErr;
}

// Builds and evaluates a SecTrustRef for the certificate chain contained
// in |cert_array|, using the verification policies in |trust_policies|. On
// success, returns OK, and updates |trust_ref|, |is_trusted|, and
// |trust_error|. On failure, no output parameters are modified.
//
// Note: An OK return does not mean that |cert_array| is trusted, merely that
// verification was performed successfully.
int BuildAndEvaluateSecTrustRef(CFArrayRef cert_array,
                                CFArrayRef trust_policies,
                                CFDataRef ocsp_response_ref,
                                CFArrayRef sct_array_ref,
                                ScopedCFTypeRef<SecTrustRef>* trust_ref,
                                ScopedCFTypeRef<CFArrayRef>* verified_chain,
                                bool* is_trusted,
                                ScopedCFTypeRef<CFErrorRef>* trust_error) {
  ScopedCFTypeRef<SecTrustRef> tmp_trust;
  OSStatus status = SecTrustCreateWithCertificates(cert_array, trust_policies,
                                                   tmp_trust.InitializeInto());
  if (status)
    return NetErrorFromOSStatus(status);

  if (TestRootCerts::HasInstance()) {
    status = TestRootCerts::GetInstance()->FixupSecTrustRef(tmp_trust.get());
    if (status)
      return NetErrorFromOSStatus(status);
  }

  if (ocsp_response_ref) {
    status = SecTrustSetOCSPResponse(tmp_trust.get(), ocsp_response_ref);
    if (status)
      return NetErrorFromOSStatus(status);
  }

  if (sct_array_ref) {
    if (__builtin_available(iOS 12.1.1, *)) {
      status = SecTrustSetSignedCertificateTimestamps(tmp_trust.get(),
                                                      sct_array_ref);
      if (status)
        return NetErrorFromOSStatus(status);
    }
  }

  ScopedCFTypeRef<CFErrorRef> tmp_error;
  bool tmp_is_trusted = false;
  if (__builtin_available(iOS 12.0, *)) {
    tmp_is_trusted =
        SecTrustEvaluateWithError(tmp_trust.get(), tmp_error.InitializeInto());
  } else {
#if !defined(__IPHONE_12_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_12_0
    SecTrustResultType tmp_trust_result;
    status = SecTrustEvaluate(tmp_trust.get(), &tmp_trust_result);
    if (status)
      return NetErrorFromOSStatus(status);
    switch (tmp_trust_result) {
      case kSecTrustResultUnspecified:
      case kSecTrustResultProceed:
        tmp_is_trusted = true;
        break;
      case kSecTrustResultInvalid:
        return ERR_FAILED;
      default:
        tmp_is_trusted = false;
    }
#endif
  }

  trust_ref->swap(tmp_trust);
  trust_error->swap(tmp_error);
  *verified_chain = x509_util::CertificateChainFromSecTrust(trust_ref->get());
  *is_trusted = tmp_is_trusted;
  return OK;
}

void GetCertChainInfo(CFArrayRef cert_chain, CertVerifyResult* verify_result) {
  DCHECK_LT(0, CFArrayGetCount(cert_chain));

  base::apple::ScopedCFTypeRef<SecCertificateRef> verified_cert;
  std::vector<base::apple::ScopedCFTypeRef<SecCertificateRef>> verified_chain;
  for (CFIndex i = 0, count = CFArrayGetCount(cert_chain); i < count; ++i) {
    SecCertificateRef chain_cert = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(cert_chain, i)));
    if (i == 0) {
      verified_cert.reset(chain_cert, base::scoped_policy::RETAIN);
    } else {
      verified_chain.emplace_back(chain_cert, base::scoped_policy::RETAIN);
    }

    base::apple::ScopedCFTypeRef<CFDataRef> der_data(
        SecCertificateCopyData(chain_cert));
    if (!der_data) {
      verify_result->cert_status |= CERT_STATUS_INVALID;
      return;
    }

    std::string_view spki_bytes;
    if (!asn1::ExtractSPKIFromDERCert(
            base::as_string_view(base::apple::CFDataToSpan(der_data.get())),
            &spki_bytes)) {
      verify_result->cert_status |= CERT_STATUS_INVALID;
      return;
    }

    HashValue sha256(HASH_VALUE_SHA256);
    CC_SHA256(spki_bytes.data(), spki_bytes.size(), sha256.data());
    verify_result->public_key_hashes.push_back(sha256);
  }
  if (!verified_cert.get()) {
    NOTREACHED_IN_MIGRATION();
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

}  // namespace

CertVerifyProcIOS::CertVerifyProcIOS(scoped_refptr<CRLSet> crl_set)
    : CertVerifyProc(std::move(crl_set)) {}

// static
CertStatus CertVerifyProcIOS::GetCertFailureStatusFromError(CFErrorRef error) {
  if (!error)
    return CERT_STATUS_INVALID;

  base::apple::ScopedCFTypeRef<CFStringRef> error_domain(
      CFErrorGetDomain(error));
  CFIndex error_code = CFErrorGetCode(error);

  if (error_domain.get() != kCFErrorDomainOSStatus) {
    LOG(WARNING) << "Unhandled error domain: " << error;
    return CERT_STATUS_INVALID;
  }

  return CertStatusFromOSStatus(error_code);
}

#if !defined(__IPHONE_12_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_12_0
// The iOS APIs don't expose an API-stable set of reasons for certificate
// validation failures. However, internally, the reason is tracked, and it's
// converted to user-facing localized strings.
//
// In the absence of a consistent API, convert the English strings to their
// localized counterpart, and then compare that with the error properties. If
// they're equal, it's a strong sign that this was the cause for the error.
// While this will break if/when iOS changes the contents of these strings,
// it's sufficient enough for now.
//
// TODO(rsleevi): https://crbug.com/601915 - Use a less brittle solution when
// possible.
// static
CertStatus CertVerifyProcIOS::GetCertFailureStatusFromTrust(SecTrustRef trust) {
  CertStatus reason = 0;

  base::apple::ScopedCFTypeRef<CFArrayRef> properties(
      SecTrustCopyProperties(trust));
  if (!properties)
    return CERT_STATUS_INVALID;

  const CFIndex properties_length = CFArrayGetCount(properties.get());
  if (properties_length == 0)
    return CERT_STATUS_INVALID;

  CFBundleRef bundle =
      CFBundleGetBundleWithIdentifier(CFSTR("com.apple.Security"));
  CFStringRef date_string =
      CFSTR("One or more certificates have expired or are not valid yet.");
  ScopedCFTypeRef<CFStringRef> date_error(CFBundleCopyLocalizedString(
      bundle, date_string, date_string, CFSTR("SecCertificate")));
  CFStringRef trust_string = CFSTR("Root certificate is not trusted.");
  ScopedCFTypeRef<CFStringRef> trust_error(CFBundleCopyLocalizedString(
      bundle, trust_string, trust_string, CFSTR("SecCertificate")));
  CFStringRef weak_string =
      CFSTR("One or more certificates is using a weak key size.");
  ScopedCFTypeRef<CFStringRef> weak_error(CFBundleCopyLocalizedString(
      bundle, weak_string, weak_string, CFSTR("SecCertificate")));
  CFStringRef hostname_mismatch_string = CFSTR("Hostname mismatch.");
  ScopedCFTypeRef<CFStringRef> hostname_mismatch_error(
      CFBundleCopyLocalizedString(bundle, hostname_mismatch_string,
                                  hostname_mismatch_string,
                                  CFSTR("SecCertificate")));
  CFStringRef root_certificate_string =
      CFSTR("Unable to build chain to root certificate.");
  ScopedCFTypeRef<CFStringRef> root_certificate_error(
      CFBundleCopyLocalizedString(bundle, root_certificate_string,
                                  root_certificate_string,
                                  CFSTR("SecCertificate")));
  CFStringRef policy_requirements_not_met_string =
      CFSTR("Policy requirements not met.");
  ScopedCFTypeRef<CFStringRef> policy_requirements_not_met_error(
      CFBundleCopyLocalizedString(bundle, policy_requirements_not_met_string,
                                  policy_requirements_not_met_string,
                                  CFSTR("SecCertificate")));

  for (CFIndex i = 0; i < properties_length; ++i) {
    CFDictionaryRef dict = reinterpret_cast<CFDictionaryRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(properties.get(), i)));
    CFStringRef error = reinterpret_cast<CFStringRef>(
        const_cast<void*>(CFDictionaryGetValue(dict, CFSTR("value"))));

    if (CFEqual(error, date_error.get())) {
      reason |= CERT_STATUS_DATE_INVALID;
    } else if (CFEqual(error, trust_error.get())) {
      reason |= CERT_STATUS_AUTHORITY_INVALID;
    } else if (CFEqual(error, weak_error.get())) {
      reason |= CERT_STATUS_WEAK_KEY;
    } else if (CFEqual(error, hostname_mismatch_error.get())) {
      reason |= CERT_STATUS_COMMON_NAME_INVALID;
    } else if (CFEqual(error, policy_requirements_not_met_error.get())) {
      reason |= CERT_STATUS_INVALID | CERT_STATUS_AUTHORITY_INVALID;
    } else if (CFEqual(error, root_certificate_error.get())) {
      reason |= CERT_STATUS_AUTHORITY_INVALID;
    } else {
      LOG(ERROR) << "Unrecognized error: " << error;
      reason |= CERT_STATUS_INVALID;
    }
  }

  return reason;
}
#endif  // !defined(__IPHONE_12_0) || __IPHONE_OS_VERSION_MIN_REQUIRED <
        // __IPHONE_12_0

CertVerifyProcIOS::~CertVerifyProcIOS() = default;

int CertVerifyProcIOS::VerifyInternal(X509Certificate* cert,
                                      const std::string& hostname,
                                      const std::string& ocsp_response,
                                      const std::string& sct_list,
                                      int flags,
                                      CertVerifyResult* verify_result,
                                      const NetLogWithSource& net_log) {
  ScopedCFTypeRef<CFArrayRef> trust_policies;
  OSStatus status = CreateTrustPolicies(&trust_policies);
  if (status)
    return NetErrorFromOSStatus(status);

  ScopedCFTypeRef<CFMutableArrayRef> cert_array(
      x509_util::CreateSecCertificateArrayForX509Certificate(
          cert, x509_util::InvalidIntermediateBehavior::kIgnore));
  if (!cert_array) {
    verify_result->cert_status |= CERT_STATUS_INVALID;
    return ERR_CERT_INVALID;
  }

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
    if (__builtin_available(iOS 12.1.1, *)) {
      std::vector<std::string_view> decoded_sct_list;
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

  ScopedCFTypeRef<SecTrustRef> trust_ref;
  bool is_trusted = false;
  ScopedCFTypeRef<CFArrayRef> final_chain;
  ScopedCFTypeRef<CFErrorRef> trust_error;

  int err = BuildAndEvaluateSecTrustRef(
      cert_array.get(), trust_policies.get(), ocsp_response_ref.get(),
      sct_array_ref.get(), &trust_ref, &final_chain, &is_trusted, &trust_error);
  if (err)
    return err;

  if (CFArrayGetCount(final_chain.get()) == 0) {
    return ERR_FAILED;
  }

  // TODO(rsleevi): Support CRLSet revocation.
  if (!is_trusted) {
    if (__builtin_available(iOS 12.0, *)) {
      verify_result->cert_status |=
          GetCertFailureStatusFromError(trust_error.get());
    } else {
#if !defined(__IPHONE_12_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_12_0
      SecTrustResultType trust_result = kSecTrustResultInvalid;
      status = SecTrustGetTrustResult(trust_ref.get(), &trust_result);
      if (status)
        return NetErrorFromOSStatus(status);
      switch (trust_result) {
        case kSecTrustResultUnspecified:
        case kSecTrustResultProceed:
          NOTREACHED_IN_MIGRATION();
          break;
        case kSecTrustResultDeny:
          verify_result->cert_status |= CERT_STATUS_AUTHORITY_INVALID;
          break;
        default:
          verify_result->cert_status |=
              GetCertFailureStatusFromTrust(trust_ref.get());
      }
#else
      // It should be impossible to reach this code, but if somehow it is
      // reached it would allow any certificate as valid since no errors would
      // be added to cert_status. Therefore, add a CHECK as a fail safe.
      CHECK(false);
#endif
    }
  }
  GetCertChainInfo(final_chain.get(), verify_result);

  // While iOS lacks the ability to distinguish system-trusted versus
  // user-installed roots, the set of roots that are expected to comply with
  // the Baseline Requirements can be determined by
  // GetNetTrustAnchorHistogramForSPKI() - a non-zero value means that it is
  // known as a publicly trusted, and therefore subject to the BRs, cert.
  for (auto it = verify_result->public_key_hashes.rbegin();
       it != verify_result->public_key_hashes.rend() &&
       !verify_result->is_issued_by_known_root;
       ++it) {
    verify_result->is_issued_by_known_root =
        GetNetTrustAnchorHistogramIdForSPKI(*it) != 0;
  }

  if (IsCertStatusError(verify_result->cert_status))
    return MapCertStatusToNetError(verify_result->cert_status);

  if (TestRootCerts::HasInstance() &&
      !verify_result->verified_cert->intermediate_buffers().empty() &&
      TestRootCerts::GetInstance()->IsKnownRoot(x509_util::CryptoBufferAsSpan(
          verify_result->verified_cert->intermediate_buffers().back().get()))) {
    verify_result->is_issued_by_known_root = true;
  }

  LogNameNormalizationMetrics(".IOS", verify_result->verified_cert.get(),
                              verify_result->is_issued_by_known_root);

  return OK;
}

}  // namespace net
