// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_X509_UTIL_MAC_H_
#define NET_CERT_X509_UTIL_MAC_H_

#include <Security/Security.h>

namespace net::x509_util {

// CSSM functions are deprecated as of OSX 10.7, but have no replacement.
// https://bugs.chromium.org/p/chromium/issues/detail?id=590914#c1
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

// Wrapper for a CSSM_DATA_PTR that was obtained via one of the CSSM field
// accessors (such as CSSM_CL_CertGet[First/Next]Value or
// CSSM_CL_CertGet[First/Next]CachedValue).
class CSSMFieldValue {
 public:
  CSSMFieldValue();
  CSSMFieldValue(CSSM_CL_HANDLE cl_handle,
                 const CSSM_OID* oid,
                 CSSM_DATA_PTR field);

  CSSMFieldValue(const CSSMFieldValue&) = delete;
  CSSMFieldValue& operator=(const CSSMFieldValue&) = delete;

  ~CSSMFieldValue();

  CSSM_OID_PTR oid() const { return oid_; }
  CSSM_DATA_PTR field() const { return field_; }

  // Returns the field as if it was an arbitrary type - most commonly, by
  // interpreting the field as a specific CSSM/CDSA parsed type, such as
  // CSSM_X509_SUBJECT_PUBLIC_KEY_INFO or CSSM_X509_ALGORITHM_IDENTIFIER.
  // An added check is applied to ensure that the current field is large
  // enough to actually contain the requested type.
  template <typename T> const T* GetAs() const {
    if (!field_ || field_->Length < sizeof(T))
      return nullptr;
    return reinterpret_cast<const T*>(field_->Data);
  }

  void Reset(CSSM_CL_HANDLE cl_handle,
             CSSM_OID_PTR oid,
             CSSM_DATA_PTR field);

 private:
  CSSM_CL_HANDLE cl_handle_;
  CSSM_OID_PTR oid_;
  CSSM_DATA_PTR field_;
};

// CSSMCachedCertificate is a container class that is used to wrap the
// CSSM_CL_CertCache APIs and provide safe and efficient access to
// certificate fields in their CSSM form.
//
// To provide efficient access to certificate/CRL fields, CSSM provides an
// API/SPI to "cache" a certificate/CRL. The exact meaning of a cached
// certificate is not defined by CSSM, but is documented to generally be some
// intermediate or parsed form of the certificate. In the case of Apple's
// CSSM CL implementation, the intermediate form is the parsed certificate
// stored in an internal format (which happens to be NSS). By caching the
// certificate, callers that wish to access multiple fields (such as subject,
// issuer, and validity dates) do not need to repeatedly parse the entire
// certificate, nor are they forced to convert all fields from their NSS types
// to their CSSM equivalents. This latter point is especially helpful when
// running on OS X 10.5, as it will fail to convert some fields that reference
// unsupported algorithms, such as ECC.
class CSSMCachedCertificate {
 public:
  CSSMCachedCertificate();
  ~CSSMCachedCertificate();

  // Initializes the CSSMCachedCertificate by caching the specified
  // |os_cert_handle|. On success, returns noErr.
  // Note: Once initialized, the cached certificate should only be accessed
  // from a single thread.
  OSStatus Init(SecCertificateRef os_cert_handle);

  // Fetches the first value for the field associated with |field_oid|.
  // If |field_oid| is a valid OID and is present in the current certificate,
  // returns CSSM_OK and stores the first value in |field|. If additional
  // values are associated with |field_oid|, they are ignored.
  OSStatus GetField(const CSSM_OID* field_oid, CSSMFieldValue* field) const;

 private:
  CSSM_CL_HANDLE cl_handle_;
  CSSM_HANDLE cached_cert_handle_;
};

// Compares two OIDs by value.
inline bool CSSMOIDEqual(const CSSM_OID* oid1, const CSSM_OID* oid2) {
  return oid1->Length == oid2->Length &&
         (memcmp(oid1->Data, oid2->Data, oid1->Length) == 0);
}

#pragma clang diagnostic pop  // "-Wdeprecated-declarations"

}  // namespace net::x509_util

#endif  // NET_CERT_X509_UTIL_MAC_H_
