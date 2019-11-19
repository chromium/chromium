// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_util_mac.h"

#include <CommonCrypto/CommonDigest.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "net/cert/x509_certificate.h"
#include "third_party/apple_apsl/cssmapplePriv.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

// CSSM functions are deprecated as of OSX 10.7, but have no replacement.
// https://bugs.chromium.org/p/chromium/issues/detail?id=590914#c1
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace x509_util {

namespace {

// Creates a SecPolicyRef for the given OID, with optional value.
OSStatus CreatePolicy(const CSSM_OID* policy_oid,
                      void* option_data,
                      size_t option_length,
                      SecPolicyRef* policy) {
  SecPolicySearchRef search;
  OSStatus err = SecPolicySearchCreate(CSSM_CERT_X_509v3, policy_oid, NULL,
                                       &search);
  if (err)
    return err;
  err = SecPolicySearchCopyNext(search, policy);
  CFRelease(search);
  if (err)
    return err;

  if (option_data) {
    CSSM_DATA options_data = {
      option_length,
      reinterpret_cast<uint8_t*>(option_data)
    };
    err = SecPolicySetValue(*policy, &options_data);
    if (err) {
      CFRelease(*policy);
      return err;
    }
  }
  return noErr;
}

}  // namespace

bool IsValidSecCertificate(SecCertificateRef cert_handle) {
  const CSSM_X509_NAME* sanity_check = NULL;
  OSStatus status = SecCertificateGetSubject(cert_handle, &sanity_check);
  return status == noErr && sanity_check;
}

base::ScopedCFTypeRef<SecCertificateRef> CreateSecCertificateFromBytes(
    const uint8_t* data,
    size_t length) {
  CSSM_DATA cert_data;
  cert_data.Data = const_cast<uint8_t*>(data);
  cert_data.Length = length;

  base::ScopedCFTypeRef<SecCertificateRef> cert_handle;
  OSStatus status = SecCertificateCreateFromData(&cert_data, CSSM_CERT_X_509v3,
                                                 CSSM_CERT_ENCODING_DER,
                                                 cert_handle.InitializeInto());
  if (status != noErr)
    return base::ScopedCFTypeRef<SecCertificateRef>();
  if (!IsValidSecCertificate(cert_handle.get()))
    return base::ScopedCFTypeRef<SecCertificateRef>();
  return cert_handle;
}

base::ScopedCFTypeRef<SecCertificateRef>
CreateSecCertificateFromX509Certificate(const X509Certificate* cert) {
  return CreateSecCertificateFromBytes(CRYPTO_BUFFER_data(cert->cert_buffer()),
                                       CRYPTO_BUFFER_len(cert->cert_buffer()));
}

scoped_refptr<X509Certificate> CreateX509CertificateFromSecCertificate(
    SecCertificateRef sec_cert,
    const std::vector<SecCertificateRef>& sec_chain) {
  return CreateX509CertificateFromSecCertificate(sec_cert, sec_chain, {});
}

scoped_refptr<X509Certificate> CreateX509CertificateFromSecCertificate(
    SecCertificateRef sec_cert,
    const std::vector<SecCertificateRef>& sec_chain,
    X509Certificate::UnsafeCreateOptions options) {
  CSSM_DATA der_data;
  if (!sec_cert || SecCertificateGetData(sec_cert, &der_data) != noErr)
    return nullptr;
  bssl::UniquePtr<CRYPTO_BUFFER> cert_handle(
      X509Certificate::CreateCertBufferFromBytes(
          reinterpret_cast<const char*>(der_data.Data), der_data.Length));
  if (!cert_handle)
    return nullptr;
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  for (const SecCertificateRef& sec_intermediate : sec_chain) {
    if (!sec_intermediate ||
        SecCertificateGetData(sec_intermediate, &der_data) != noErr) {
      return nullptr;
    }
    bssl::UniquePtr<CRYPTO_BUFFER> intermediate_cert_handle(
        X509Certificate::CreateCertBufferFromBytes(
            reinterpret_cast<const char*>(der_data.Data), der_data.Length));
    if (!intermediate_cert_handle)
      return nullptr;
    intermediates.push_back(std::move(intermediate_cert_handle));
  }
  scoped_refptr<X509Certificate> result(
      X509Certificate::CreateFromBufferUnsafeOptions(
          std::move(cert_handle), std::move(intermediates), options));
  return result;
}

SHA256HashValue CalculateFingerprint256(SecCertificateRef cert) {
  SHA256HashValue sha256;
  memset(sha256.data, 0, sizeof(sha256.data));

  CSSM_DATA cert_data;
  OSStatus status = SecCertificateGetData(cert, &cert_data);
  if (status)
    return sha256;

  DCHECK(cert_data.Data);
  DCHECK_NE(cert_data.Length, 0U);

  CC_SHA256(cert_data.Data, cert_data.Length, sha256.data);

  return sha256;
}

OSStatus CreateSSLClientPolicy(SecPolicyRef* policy) {
  *policy = SecPolicyCreateSSL(false /* server */, nullptr);
  return *policy ? noErr : errSecNoPolicyModule;
}

OSStatus CreateSSLServerPolicy(const std::string& hostname,
                               SecPolicyRef* policy) {
  base::ScopedCFTypeRef<CFStringRef> hostname_cfstring;
  if (!hostname.empty()) {
    hostname_cfstring.reset(base::SysUTF8ToCFStringRef(hostname));
    if (!hostname_cfstring)
      return errSecNoPolicyModule;
  }

  *policy = SecPolicyCreateSSL(true /* server */, hostname_cfstring.get());
  return *policy ? noErr : errSecNoPolicyModule;
}

OSStatus CreateBasicX509Policy(SecPolicyRef* policy) {
  *policy = SecPolicyCreateBasicX509();
  return *policy ? noErr : errSecNoPolicyModule;
}

OSStatus CreateRevocationPolicies(bool enable_revocation_checking,
                                  CFMutableArrayRef policies) {
  if (__builtin_available(macos 10.12, *)) {
    // On Sierra, it's not possible to disable network revocation checking
    // without also breaking AIA. If revocation checking isn't explicitly
    // enabled, just don't add a revocation policy.
    if (!enable_revocation_checking)
      return noErr;

    // If revocation checking is requested, enable checking and require positive
    // results. Note that this will fail if there are certs with no
    // CRLDistributionPoints or OCSP AIA urls, which differs from the behavior
    // of |enable_revocation_checking| on pre-10.12. There does not appear to be
    // a way around this, but it shouldn't matter much in practice since
    // revocation checking is generally used with EV certs, where it is expected
    // that all certs include revocation mechanisms.
    SecPolicyRef revocation_policy =
        SecPolicyCreateRevocation(kSecRevocationUseAnyAvailableMethod |
                                  kSecRevocationRequirePositiveResponse);

    if (!revocation_policy)
      return errSecNoPolicyModule;
    CFArrayAppendValue(policies, revocation_policy);
    CFRelease(revocation_policy);
    return noErr;
  }
  OSStatus status = noErr;

  // In order to bypass the system revocation checking settings, the
  // SecTrustRef must have at least one revocation policy associated with it.
  // Since it is not known prior to verification whether the Apple TP will
  // consider a certificate as an EV candidate, the default policy used is a
  // CRL policy, since it does not communicate over the network.
  // If the TP believes the leaf is an EV cert, it will explicitly add an
  // OCSP policy to perform the online checking, and if it doesn't believe
  // that the leaf is EV, then the default CRL policy will effectively no-op.
  // This behaviour is used to implement EV-only revocation checking.
  if (enable_revocation_checking) {
    CSSM_APPLE_TP_CRL_OPTIONS tp_crl_options;
    memset(&tp_crl_options, 0, sizeof(tp_crl_options));
    tp_crl_options.Version = CSSM_APPLE_TP_CRL_OPTS_VERSION;
    // Only allow network CRL fetches if the caller explicitly requests
    // online revocation checking. Note that, as of OS X 10.7.2, the system
    // will set force this flag on according to system policies, so
    // online revocation checks cannot be completely disabled.
    // Starting with OS X 10.12, if a CRL policy is added without the
    // FETCH_CRL_FROM_NET flag, AIA fetching is disabled.
    tp_crl_options.CrlFlags = CSSM_TP_ACTION_FETCH_CRL_FROM_NET;

    SecPolicyRef crl_policy;
    status = CreatePolicy(&CSSMOID_APPLE_TP_REVOCATION_CRL, &tp_crl_options,
                          sizeof(tp_crl_options), &crl_policy);
    if (status)
      return status;
    CFArrayAppendValue(policies, crl_policy);
    CFRelease(crl_policy);
  }

  // If revocation checking is explicitly enabled, then add an OCSP policy
  // and allow network access. If both revocation checking is
  // disabled, then the added OCSP policy will be prevented from
  // accessing the network. This is done because the TP will force an OCSP
  // policy to be present when it believes the certificate is EV.
  CSSM_APPLE_TP_OCSP_OPTIONS tp_ocsp_options;
  memset(&tp_ocsp_options, 0, sizeof(tp_ocsp_options));
  tp_ocsp_options.Version = CSSM_APPLE_TP_OCSP_OPTS_VERSION;

  if (enable_revocation_checking) {
    // The default for the OCSP policy is to fetch responses via the network,
    // unlike the CRL policy default. The policy is further modified to
    // prefer OCSP over CRLs, if both are specified on the certificate. This
    // is because an OCSP response is both sufficient and typically
    // significantly smaller than the CRL counterpart.
    tp_ocsp_options.Flags = CSSM_TP_ACTION_OCSP_SUFFICIENT;
  } else {
    // Effectively disable OCSP checking by making it impossible to get an
    // OCSP response. Even if the Apple TP forces OCSP, no checking will
    // be able to succeed. If this happens, the Apple TP will report an error
    // that OCSP was unavailable, but this will be handled and suppressed in
    // X509Certificate::Verify().
    tp_ocsp_options.Flags = CSSM_TP_ACTION_OCSP_DISABLE_NET |
                            CSSM_TP_ACTION_OCSP_CACHE_READ_DISABLE;
  }

  SecPolicyRef ocsp_policy;
  status = CreatePolicy(&CSSMOID_APPLE_TP_REVOCATION_OCSP, &tp_ocsp_options,
                        sizeof(tp_ocsp_options), &ocsp_policy);
  if (status)
    return status;
  CFArrayAppendValue(policies, ocsp_policy);
  CFRelease(ocsp_policy);

  return status;
}

CSSMFieldValue::CSSMFieldValue()
    : cl_handle_(CSSM_INVALID_HANDLE),
      oid_(NULL),
      field_(NULL) {
}
CSSMFieldValue::CSSMFieldValue(CSSM_CL_HANDLE cl_handle,
                               const CSSM_OID* oid,
                               CSSM_DATA_PTR field)
    : cl_handle_(cl_handle),
      oid_(const_cast<CSSM_OID_PTR>(oid)),
      field_(field) {
}

CSSMFieldValue::~CSSMFieldValue() {
  Reset(CSSM_INVALID_HANDLE, NULL, NULL);
}

void CSSMFieldValue::Reset(CSSM_CL_HANDLE cl_handle,
                           CSSM_OID_PTR oid,
                           CSSM_DATA_PTR field) {
  if (cl_handle_ && oid_ && field_)
    CSSM_CL_FreeFieldValue(cl_handle_, oid_, field_);
  cl_handle_ = cl_handle;
  oid_ = oid;
  field_ = field;
}

CSSMCachedCertificate::CSSMCachedCertificate()
    : cl_handle_(CSSM_INVALID_HANDLE),
      cached_cert_handle_(CSSM_INVALID_HANDLE) {
}
CSSMCachedCertificate::~CSSMCachedCertificate() {
  if (cl_handle_ && cached_cert_handle_)
    CSSM_CL_CertAbortCache(cl_handle_, cached_cert_handle_);
}

OSStatus CSSMCachedCertificate::Init(SecCertificateRef os_cert_handle) {
  DCHECK(!cl_handle_ && !cached_cert_handle_);
  DCHECK(os_cert_handle);
  CSSM_DATA cert_data;
  OSStatus status = SecCertificateGetData(os_cert_handle, &cert_data);
  if (status)
    return status;
  status = SecCertificateGetCLHandle(os_cert_handle, &cl_handle_);
  if (status) {
    DCHECK(!cl_handle_);
    return status;
  }

  status = CSSM_CL_CertCache(cl_handle_, &cert_data, &cached_cert_handle_);
  if (status)
    DCHECK(!cached_cert_handle_);
  return status;
}

OSStatus CSSMCachedCertificate::GetField(const CSSM_OID* field_oid,
                                         CSSMFieldValue* field) const {
  DCHECK(cl_handle_);
  DCHECK(cached_cert_handle_);

  CSSM_OID_PTR oid = const_cast<CSSM_OID_PTR>(field_oid);
  CSSM_DATA_PTR field_ptr = NULL;
  CSSM_HANDLE results_handle = CSSM_INVALID_HANDLE;
  uint32_t field_value_count = 0;
  CSSM_RETURN status = CSSM_CL_CertGetFirstCachedFieldValue(
      cl_handle_, cached_cert_handle_, oid, &results_handle,
      &field_value_count, &field_ptr);
  if (status)
    return status;

  // Note: |field_value_count| may be > 1, indicating that more than one
  // value is present. This may happen with extensions, but for current
  // usages, only the first value is returned.
  CSSM_CL_CertAbortQuery(cl_handle_, results_handle);
  field->Reset(cl_handle_, oid, field_ptr);
  return CSSM_OK;
}

}  // namespace x509_util

#pragma clang diagnostic pop  // "-Wdeprecated-declarations"

}  // namespace net
