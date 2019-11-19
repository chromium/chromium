// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CT_VERIFIER_H_
#define NET_CERT_CT_VERIFIER_H_

#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"

namespace net {

class NetLogWithSource;
class X509Certificate;

// Interface for verifying Signed Certificate Timestamps over a certificate.
class NET_EXPORT CTVerifier {
 public:
  virtual ~CTVerifier() {}

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
  // The |hostname| (or IP address literal) of the server that presented |cert|
  // must be provided so that inclusion checks for |cert| are able to avoid
  // leaking information about which servers have been visited.
  virtual void Verify(base::StringPiece hostname,
                      X509Certificate* cert,
                      base::StringPiece stapled_ocsp_response,
                      base::StringPiece sct_list_from_tls_extension,
                      SignedCertificateTimestampAndStatusList* output_scts,
                      const NetLogWithSource& net_log) = 0;
};

}  // namespace net

#endif  // NET_CERT_CT_VERIFIER_H_
