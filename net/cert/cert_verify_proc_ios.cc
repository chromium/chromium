// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_ios.h"

#include <CommonCrypto/CommonDigest.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/known_roots.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_ios.h"
#include "net/cert/x509_util_ios_and_mac.h"

using base::ScopedCFTypeRef;

extern "C" {
// Declared in <Security/SecTrust.h>, available in iOS 12.1.1+
// TODO(mattm): Remove this weak_import once chromium requires a new enough
// iOS SDK.
OSStatus SecTrustSetSignedCertificateTimestamps(SecTrustRef, CFArrayRef)
    __attribute__((weak_import));
}  // extern "C"

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

  SecPolicyRef ssl_policy = SecPolicyCreateBasicX509();
  CFArrayAppendValue(local_policies, ssl_policy);
  CFRelease(ssl_policy);
  ssl_policy = SecPolicyCreateSSL(true, nullptr);
  CFArrayAppendValue(local_policies, ssl_policy);
  CFRelease(ssl_policy);

  policies->reset(local_policies.release());
  return noErr;
}

// Builds and evaluates a SecTrustRef for the certificate chain contained
// in |cert_array|, using the verification policies in |trust_policies|. On
// success, returns OK, and updates |trust_ref| and |trust_result|. On failure,
// no output parameters are modified.
//
// Note: An OK return does not mean that |cert_array| is trusted, merely that
// verification was performed successfully.
int BuildAndEvaluateSecTrustRef(CFArrayRef cert_array,
                                CFArrayRef trust_policies,
                                CFDataRef ocsp_response_ref,
                                CFArrayRef sct_array_ref,
                                ScopedCFTypeRef<SecTrustRef>* trust_ref,
                                ScopedCFTypeRef<CFArrayRef>* verified_chain,
                                SecTrustResultType* trust_result) {
  SecTrustRef tmp_trust = nullptr;
  OSStatus status =
      SecTrustCreateWithCertificates(cert_array, trust_policies, &tmp_trust);
  if (status)
    return NetErrorFromOSStatus(status);
  ScopedCFTypeRef<SecTrustRef> scoped_tmp_trust(tmp_trust);

  if (TestRootCerts::HasInstance()) {
    status = TestRootCerts::GetInstance()->FixupSecTrustRef(tmp_trust);
    if (status)
      return NetErrorFromOSStatus(status);
  }

  if (ocsp_response_ref) {
    status = SecTrustSetOCSPResponse(tmp_trust, ocsp_response_ref);
    if (status)
      return NetErrorFromOSStatus(status);
  }

  if (sct_array_ref) {
    if (__builtin_available(iOS 12.1.1, *)) {
      status = SecTrustSetSignedCertificateTimestamps(tmp_trust, sct_array_ref);
      if (status)
        return NetErrorFromOSStatus(status);
    }
  }

  SecTrustResultType tmp_trust_result;
  status = SecTrustEvaluate(tmp_trust, &tmp_trust_result);
  if (status)
    return NetErrorFromOSStatus(status);

  ScopedCFTypeRef<CFMutableArrayRef> tmp_verified_chain(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  const CFIndex chain_length = SecTrustGetCertificateCount(tmp_trust);
  for (CFIndex i = 0; i < chain_length; ++i) {
    SecCertificateRef chain_cert = SecTrustGetCertificateAtIndex(tmp_trust, i);
    CFArrayAppendValue(tmp_verified_chain, chain_cert);
  }

  trust_ref->swap(scoped_tmp_trust);
  *trust_result = tmp_trust_result;
  verified_chain->reset(tmp_verified_chain.release());
  return OK;
}

void GetCertChainInfo(CFArrayRef cert_chain, CertVerifyResult* verify_result) {
  DCHECK_LT(0, CFArrayGetCount(cert_chain));

  SecCertificateRef verified_cert = nullptr;
  std::vector<SecCertificateRef> verified_chain;
  for (CFIndex i = 0, count = CFArrayGetCount(cert_chain); i < count; ++i) {
    SecCertificateRef chain_cert = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(cert_chain, i)));
    if (i == 0) {
      verified_cert = chain_cert;
    } else {
      verified_chain.push_back(chain_cert);
    }

    base::ScopedCFTypeRef<CFDataRef> der_data(
        SecCertificateCopyData(chain_cert));
    if (!der_data) {
      verify_result->cert_status |= CERT_STATUS_INVALID;
      return;
    }

    base::StringPiece spki_bytes;
    if (!asn1::ExtractSPKIFromDERCert(
            base::StringPiece(
                reinterpret_cast<const char*>(CFDataGetBytePtr(der_data)),
                CFDataGetLength(der_data)),
            &spki_bytes)) {
      verify_result->cert_status |= CERT_STATUS_INVALID;
      return;
    }

    HashValue sha256(HASH_VALUE_SHA256);
    CC_SHA256(spki_bytes.data(), spki_bytes.size(), sha256.data());
    verify_result->public_key_hashes.push_back(sha256);
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

}  // namespace

CertVerifyProcIOS::CertVerifyProcIOS() {}

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

  base::ScopedCFTypeRef<CFArrayRef> properties(SecTrustCopyProperties(trust));
  if (!properties)
    return CERT_STATUS_INVALID;

  const CFIndex properties_length = CFArrayGetCount(properties);
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
        const_cast<void*>(CFArrayGetValueAtIndex(properties, i)));
    CFStringRef error = reinterpret_cast<CFStringRef>(
        const_cast<void*>(CFDictionaryGetValue(dict, CFSTR("value"))));

    if (CFEqual(error, date_error)) {
      reason |= CERT_STATUS_DATE_INVALID;
    } else if (CFEqual(error, trust_error)) {
      reason |= CERT_STATUS_AUTHORITY_INVALID;
    } else if (CFEqual(error, weak_error)) {
      reason |= CERT_STATUS_WEAK_KEY;
    } else if (CFEqual(error, hostname_mismatch_error)) {
      reason |= CERT_STATUS_COMMON_NAME_INVALID;
    } else if (CFEqual(error, policy_requirements_not_met_error)) {
      reason |= CERT_STATUS_INVALID | CERT_STATUS_AUTHORITY_INVALID;
    } else if (CFEqual(error, root_certificate_error)) {
      reason |= CERT_STATUS_AUTHORITY_INVALID;
    } else {
      LOG(ERROR) << "Unrecognized error: " << error;
      reason |= CERT_STATUS_INVALID;
    }
  }

  return reason;
}

bool CertVerifyProcIOS::SupportsAdditionalTrustAnchors() const {
  return false;
}

CertVerifyProcIOS::~CertVerifyProcIOS() = default;

int CertVerifyProcIOS::VerifyInternal(
    X509Certificate* cert,
    const std::string& hostname,
    const std::string& ocsp_response,
    const std::string& sct_list,
    int flags,
    CRLSet* crl_set,
    const CertificateList& additional_trust_anchors,
    CertVerifyResult* verify_result) {
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

  ScopedCFTypeRef<SecTrustRef> trust_ref;
  SecTrustResultType trust_result = kSecTrustResultDeny;
  ScopedCFTypeRef<CFArrayRef> final_chain;

  status = BuildAndEvaluateSecTrustRef(
      cert_array, trust_policies, ocsp_response_ref.get(), sct_array_ref.get(),
      &trust_ref, &final_chain, &trust_result);
  if (status)
    return NetErrorFromOSStatus(status);

  if (CFArrayGetCount(final_chain) == 0)
    return ERR_FAILED;

  // TODO(rsleevi): Support CRLSet revocation.
  switch (trust_result) {
    case kSecTrustResultUnspecified:
    case kSecTrustResultProceed:
      break;
    case kSecTrustResultDeny:
      verify_result->cert_status |= CERT_STATUS_AUTHORITY_INVALID;
      break;
    default:
      verify_result->cert_status |= GetCertFailureStatusFromTrust(trust_ref);
  }

  GetCertChainInfo(final_chain, verify_result);

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

  LogNameNormalizationMetrics(".IOS", verify_result->verified_cert.get(),
                              verify_result->is_issued_by_known_root);

  return OK;
}

}  // namespace net
