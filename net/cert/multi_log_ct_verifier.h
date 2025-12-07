// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_MULTI_LOG_CT_VERIFIER_H_
#define NET_CERT_MULTI_LOG_CT_VERIFIER_H_

#include <map>
#include <string>
#include <string_view>

#include "base/memory/scoped_refptr.h"
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
  explicit MultiLogCTVerifier(
      const std::vector<scoped_refptr<const CTLogVerifier>>& log_verifiers);

  MultiLogCTVerifier(const MultiLogCTVerifier&) = delete;
  MultiLogCTVerifier& operator=(const MultiLogCTVerifier&) = delete;

  ~MultiLogCTVerifier() override;

  // CTVerifier implementation:
  void Verify(X509Certificate* cert,
              std::string_view stapled_ocsp_response,
              std::string_view sct_list_from_tls_extension,
              base::Time current_time,
              SignedCertificateTimestampAndStatusList* output_scts,
              const NetLogWithSource& net_log) const override;

 private:
  // Verify a list of SCTs from |encoded_sct_list| over |expected_entry|,
  // placing the verification results in |output_scts|. The SCTs in the list
  // come from |origin| (as will be indicated in the origin field of each SCT).
  void VerifySCTs(std::string_view encoded_sct_list,
                  const ct::SignedEntryData& expected_entry,
                  ct::SignedCertificateTimestamp::Origin origin,
                  base::Time current_time,
                  X509Certificate* cert,
                  SignedCertificateTimestampAndStatusList* output_scts) const;

  // Verifies a single, parsed SCT against all logs.
  bool VerifySingleSCT(
      scoped_refptr<ct::SignedCertificateTimestamp> sct,
      const ct::SignedEntryData& expected_entry,
      base::Time current_time,
      X509Certificate* cert,
      SignedCertificateTimestampAndStatusList* output_scts) const;

  // Mapping from a log's ID to the verifier for this log.
  // A log's ID is the SHA-256 of the log's key, as defined in section 3.2.
  // of RFC6962.
  const std::map<std::string, scoped_refptr<const CTLogVerifier>> logs_;
};

}  // namespace net

#endif  // NET_CERT_MULTI_LOG_CT_VERIFIER_H_
