// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_util_mac.h"

#include <CommonCrypto/CommonDigest.h>

#include "base/check_op.h"
#include "base/strings/sys_string_conversions.h"
#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

// CSSM functions are deprecated as of OSX 10.7, but have no replacement.
// https://bugs.chromium.org/p/chromium/issues/detail?id=590914#c1
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace x509_util {

scoped_refptr<X509Certificate> CreateX509CertificateFromSecCertificate(
    base::ScopedCFTypeRef<SecCertificateRef> sec_cert,
    const std::vector<base::ScopedCFTypeRef<SecCertificateRef>>& sec_chain) {
  return CreateX509CertificateFromSecCertificate(sec_cert, sec_chain, {});
}

scoped_refptr<X509Certificate> CreateX509CertificateFromSecCertificate(
    base::ScopedCFTypeRef<SecCertificateRef> sec_cert,
    const std::vector<base::ScopedCFTypeRef<SecCertificateRef>>& sec_chain,
    X509Certificate::UnsafeCreateOptions options) {
  CSSM_DATA der_data;
  if (!sec_cert || SecCertificateGetData(sec_cert, &der_data) != noErr)
    return nullptr;
  bssl::UniquePtr<CRYPTO_BUFFER> cert_handle(
      X509Certificate::CreateCertBufferFromBytes(
          base::make_span(der_data.Data, der_data.Length)));
  if (!cert_handle)
    return nullptr;
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  for (const auto& sec_intermediate : sec_chain) {
    if (!sec_intermediate ||
        SecCertificateGetData(sec_intermediate, &der_data) != noErr) {
      return nullptr;
    }
    bssl::UniquePtr<CRYPTO_BUFFER> intermediate_cert_handle(
        X509Certificate::CreateCertBufferFromBytes(
            base::make_span(der_data.Data, der_data.Length)));
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
