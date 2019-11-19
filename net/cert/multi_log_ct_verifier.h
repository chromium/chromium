// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_MULTI_LOG_CT_VERIFIER_H_
#define NET_CERT_MULTI_LOG_CT_VERIFIER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/signed_certificate_timestamp.h"

namespace net {

namespace ct {
struct SignedEntryData;
}  // namespace ct

class CTLogVerifier;

// A Certificate Transparency verifier that can verify Signed Certificate
// Timestamps from multiple logs.
// It must be initialized with a list of logs by calling AddLogs.
class NET_EXPORT MultiLogCTVerifier : public CTVerifier {
 public:
  MultiLogCTVerifier();
  ~MultiLogCTVerifier() override;

  void AddLogs(
      const std::vector<scoped_refptr<const CTLogVerifier>>& log_verifiers);

  // CTVerifier implementation:
  void Verify(base::StringPiece hostname,
              X509Certificate* cert,
              base::StringPiece stapled_ocsp_response,
              base::StringPiece sct_list_from_tls_extension,
              SignedCertificateTimestampAndStatusList* output_scts,
              const NetLogWithSource& net_log) override;

 private:
  // Verify a list of SCTs from |encoded_sct_list| over |expected_entry|,
  // placing the verification results in |output_scts|. The SCTs in the list
  // come from |origin| (as will be indicated in the origin field of each SCT).
  void VerifySCTs(base::StringPiece hostname,
                  base::StringPiece encoded_sct_list,
                  const ct::SignedEntryData& expected_entry,
                  ct::SignedCertificateTimestamp::Origin origin,
                  X509Certificate* cert,
                  SignedCertificateTimestampAndStatusList* output_scts);

  // Verifies a single, parsed SCT against all logs.
  bool VerifySingleSCT(base::StringPiece hostname,
                       scoped_refptr<ct::SignedCertificateTimestamp> sct,
                       const ct::SignedEntryData& expected_entry,
                       X509Certificate* cert,
                       SignedCertificateTimestampAndStatusList* output_scts);

  // Mapping from a log's ID to the verifier for this log.
  // A log's ID is the SHA-256 of the log's key, as defined in section 3.2.
  // of RFC6962.
  std::map<std::string, scoped_refptr<const CTLogVerifier>> logs_;

  DISALLOW_COPY_AND_ASSIGN(MultiLogCTVerifier);
};

}  // namespace net

#endif  // NET_CERT_MULTI_LOG_CT_VERIFIER_H_
