// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PRIVATE_PPB_X509_UTIL_SHARED_H_
#define PPAPI_SHARED_IMPL_PRIVATE_PPB_X509_UTIL_SHARED_H_

#include "ppapi/shared_impl/private/ppb_x509_certificate_private_shared.h"

namespace net {
class X509Certificate;
}

namespace ppapi {

// Contains X509 parsing utilities that are shared between the proxy and the
// browser.
class PPAPI_SHARED_EXPORT PPB_X509Util_Shared {
 public:
  // Extracts the certificate field data from a net::X509Certificate into
  // PPB_X509Certificate_Fields.
  static bool GetCertificateFields(const net::X509Certificate& cert,
                                   ppapi::PPB_X509Certificate_Fields* fields);

  // Extracts the certificate field data from the DER representation of a
  // certificate into PPB_X509Certificate_Fields.
  static bool GetCertificateFields(const char* der,
                                   size_t length,
                                   ppapi::PPB_X509Certificate_Fields* fields);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PRIVATE_PPB_X509_UTIL_SHARED_H_
