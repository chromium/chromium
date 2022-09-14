// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_util_mac.h"

#include "base/check_op.h"

namespace net {

// CSSM functions are deprecated as of OSX 10.7, but have no replacement.
// https://bugs.chromium.org/p/chromium/issues/detail?id=590914#c1
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace x509_util {

CSSMFieldValue::CSSMFieldValue()
    : cl_handle_(CSSM_INVALID_HANDLE), oid_(nullptr), field_(nullptr) {}
CSSMFieldValue::CSSMFieldValue(CSSM_CL_HANDLE cl_handle,
                               const CSSM_OID* oid,
                               CSSM_DATA_PTR field)
    : cl_handle_(cl_handle),
      oid_(const_cast<CSSM_OID_PTR>(oid)),
      field_(field) {
}

CSSMFieldValue::~CSSMFieldValue() {
  Reset(CSSM_INVALID_HANDLE, nullptr, nullptr);
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
  CSSM_DATA_PTR field_ptr = nullptr;
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
