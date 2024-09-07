// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/multi_log_ct_verifier.h"

#include <string_view>
#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_objects_extractor.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/ct_signed_certificate_timestamp_log_param.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"

namespace net {

namespace {

// Record SCT verification status. This metric would help detecting presence
// of unknown CT logs as well as bad deployments (invalid SCTs).
void LogSCTStatusToUMA(ct::SCTVerifyStatus status) {
  // Note SCT_STATUS_MAX + 1 is passed to the UMA_HISTOGRAM_ENUMERATION as that
  // macro requires the values to be strictly less than the boundary value,
  // and SCT_STATUS_MAX is the last valid value of the SCTVerifyStatus enum
  // (since that enum is used for IPC as well).
  UMA_HISTOGRAM_ENUMERATION("Net.CertificateTransparency.SCTStatus", status,
                            ct::SCT_STATUS_MAX + 1);
}

// Record SCT origin enum. This metric measure the popularity
// of the various channels of providing SCTs for a certificate.
void LogSCTOriginToUMA(ct::SignedCertificateTimestamp::Origin origin) {
  UMA_HISTOGRAM_ENUMERATION("Net.CertificateTransparency.SCTOrigin",
                            origin,
                            ct::SignedCertificateTimestamp::SCT_ORIGIN_MAX);
}

void AddSCTAndLogStatus(scoped_refptr<ct::SignedCertificateTimestamp> sct,
                        ct::SCTVerifyStatus status,
                        SignedCertificateTimestampAndStatusList* sct_list) {
  LogSCTStatusToUMA(status);
  sct_list->push_back(SignedCertificateTimestampAndStatus(sct, status));
}

std::map<std::string, scoped_refptr<const CTLogVerifier>> CreateLogsMap(
    const std::vector<scoped_refptr<const CTLogVerifier>>& log_verifiers) {
  std::map<std::string, scoped_refptr<const CTLogVerifier>> logs;
  for (const auto& log_verifier : log_verifiers) {
    std::string key_id = log_verifier->key_id();
    logs[key_id] = log_verifier;
  }
  return logs;
}

}  // namespace

MultiLogCTVerifier::MultiLogCTVerifier(
    const std::vector<scoped_refptr<const CTLogVerifier>>& log_verifiers)
    : logs_(CreateLogsMap(log_verifiers)) {}

MultiLogCTVerifier::~MultiLogCTVerifier() = default;

void MultiLogCTVerifier::Verify(
    X509Certificate* cert,
    std::string_view stapled_ocsp_response,
    std::string_view sct_list_from_tls_extension,
    base::Time current_time,
    SignedCertificateTimestampAndStatusList* output_scts,
    const NetLogWithSource& net_log) const {
  DCHECK(cert);
  DCHECK(output_scts);

  output_scts->clear();

  std::string embedded_scts;
  if (!cert->intermediate_buffers().empty() &&
      ct::ExtractEmbeddedSCTList(cert->cert_buffer(), &embedded_scts)) {
    ct::SignedEntryData precert_entry;

    if (ct::GetPrecertSignedEntry(cert->cert_buffer(),
                                  cert->intermediate_buffers().front().get(),
                                  &precert_entry)) {
      VerifySCTs(embedded_scts, precert_entry,
                 ct::SignedCertificateTimestamp::SCT_EMBEDDED, current_time,
                 cert, output_scts);
    }
  }

  std::string sct_list_from_ocsp;
  if (!stapled_ocsp_response.empty() && !cert->intermediate_buffers().empty()) {
    ct::ExtractSCTListFromOCSPResponse(
        cert->intermediate_buffers().front().get(), cert->serial_number(),
        stapled_ocsp_response, &sct_list_from_ocsp);
  }

  // Log to Net Log, after extracting SCTs but before possibly failing on
  // X.509 entry creation.
  net_log.AddEvent(
      NetLogEventType::SIGNED_CERTIFICATE_TIMESTAMPS_RECEIVED, [&] {
        return NetLogRawSignedCertificateTimestampParams(
            embedded_scts, sct_list_from_ocsp, sct_list_from_tls_extension);
      });

  ct::SignedEntryData x509_entry;
  if (ct::GetX509SignedEntry(cert->cert_buffer(), &x509_entry)) {
    VerifySCTs(sct_list_from_ocsp, x509_entry,
               ct::SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE,
               current_time, cert, output_scts);

    VerifySCTs(sct_list_from_tls_extension, x509_entry,
               ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
               current_time, cert, output_scts);
  }

  net_log.AddEvent(NetLogEventType::SIGNED_CERTIFICATE_TIMESTAMPS_CHECKED, [&] {
    return NetLogSignedCertificateTimestampParams(output_scts);
  });
}

void MultiLogCTVerifier::VerifySCTs(
    std::string_view encoded_sct_list,
    const ct::SignedEntryData& expected_entry,
    ct::SignedCertificateTimestamp::Origin origin,
    base::Time current_time,
    X509Certificate* cert,
    SignedCertificateTimestampAndStatusList* output_scts) const {
  if (logs_.empty())
    return;

  std::vector<std::string_view> sct_list;

  if (!ct::DecodeSCTList(encoded_sct_list, &sct_list))
    return;

  for (std::vector<std::string_view>::const_iterator it = sct_list.begin();
       it != sct_list.end(); ++it) {
    std::string_view encoded_sct(*it);
    LogSCTOriginToUMA(origin);

    scoped_refptr<ct::SignedCertificateTimestamp> decoded_sct;
    if (!DecodeSignedCertificateTimestamp(&encoded_sct, &decoded_sct)) {
      LogSCTStatusToUMA(ct::SCT_STATUS_NONE);
      continue;
    }
    decoded_sct->origin = origin;

    VerifySingleSCT(decoded_sct, expected_entry, current_time, cert,
                    output_scts);
  }
}

bool MultiLogCTVerifier::VerifySingleSCT(
    scoped_refptr<ct::SignedCertificateTimestamp> sct,
    const ct::SignedEntryData& expected_entry,
    base::Time current_time,
    X509Certificate* cert,
    SignedCertificateTimestampAndStatusList* output_scts) const {
  // Assume this SCT is untrusted until proven otherwise.
  const auto& it = logs_.find(sct->log_id);
  if (it == logs_.end()) {
    AddSCTAndLogStatus(sct, ct::SCT_STATUS_LOG_UNKNOWN, output_scts);
    return false;
  }

  sct->log_description = it->second->description();

  if (!it->second->Verify(expected_entry, *sct.get())) {
    AddSCTAndLogStatus(sct, ct::SCT_STATUS_INVALID_SIGNATURE, output_scts);
    return false;
  }

  // SCT verified ok, just make sure the timestamp is legitimate.
  if (sct->timestamp > current_time) {
    AddSCTAndLogStatus(sct, ct::SCT_STATUS_INVALID_TIMESTAMP, output_scts);
    return false;
  }

  AddSCTAndLogStatus(sct, ct::SCT_STATUS_OK, output_scts);
  return true;
}

} // namespace net
