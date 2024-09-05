// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CT_VERIFIER_H_
#define NET_CERT_CT_VERIFIER_H_

#include <string_view>

#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"

namespace net {

class NetLogWithSource;
class X509Certificate;

// Interface for verifying Signed Certificate Timestamps over a certificate.
class NET_EXPORT CTVerifier {
 public:
  virtual ~CTVerifier() = default;

  // Verifies SCTs embedded in the certificate itself, SCTs embedded in a
  // stapled OCSP response, and SCTs obtained via the
  // signed_certificate_timestamp TLS extension on the given |cert|.
  // A certificate is permitted but not required to use multiple sources for
  // SCTs. It is expected that most certificates will use only one source
  // (embedding, TLS extension or OCSP stapling). If no stapled OCSP response
  // is available, |stapled_ocsp_response| should be an empty string. If no SCT
  // TLS extension was negotiated, |sct_list_from_tls_extension| should be an
  // empty string. |output_scts| will be cleared and filled with the SCTs
  // present, if any, along with their verification results.
  virtual void Verify(X509Certificate* cert,
                      std::string_view stapled_ocsp_response,
                      std::string_view sct_list_from_tls_extension,
                      base::Time current_time,
                      SignedCertificateTimestampAndStatusList* output_scts,
                      const NetLogWithSource& net_log) const = 0;
};

}  // namespace net

#endif  // NET_CERT_CT_VERIFIER_H_
