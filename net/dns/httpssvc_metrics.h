// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HTTPSSVC_METRICS_H_
#define NET_DNS_HTTPSSVC_METRICS_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. (See HttpssvcDnsRcode in
// tools/metrics/histograms/enums.xml.)
enum HttpssvcDnsRcode {
  kTimedOut = 0,
  kUnrecognizedRcode,
  kMissingDnsResponse,
  kNoError,
  kFormErr,
  kServFail,
  kNxDomain,
  kNotImp,
  kRefused,
  kMaxValue = kRefused,
};

// Helper that classifies domains as experimental, control, or other. Queries
// feature params and caches result to avoid repeated parsing.
class NET_EXPORT_PRIVATE HttpssvcExperimentDomainCache {
 public:
  HttpssvcExperimentDomainCache();
  ~HttpssvcExperimentDomainCache();
  bool IsExperimental(base::StringPiece domain);
  bool IsControl(base::StringPiece domain);

 private:
  bool ListContainsDomain(
      const std::string& domain_list,
      base::StringPiece domain,
      absl::optional<base::flat_set<std::string>>& in_out_cached_list);

  absl::optional<base::flat_set<std::string>> experimental_list_;
  absl::optional<base::flat_set<std::string>> control_list_;
};

// Translate an RCODE value to the |HttpssvcDnsRcode| enum, which is used for
// HTTPSSVC experimentation. The goal is to keep these values in a small,
// contiguous range in order to satisfy the UMA enumeration function's
// requirements. This function never returns |kTimedOut| |kUnrecognizedRcode|,
// or |kMissingDnsResponse|.
enum HttpssvcDnsRcode TranslateDnsRcodeForHttpssvcExperiment(uint8_t rcode);

// Tool for aggregating HTTPSSVC and INTEGRITY metrics. Accumulates metrics via
// the Save* methods. Records metrics to UMA on destruction.
class NET_EXPORT_PRIVATE HttpssvcMetrics {
 public:
  HttpssvcMetrics(bool secure, bool expect_intact);
  ~HttpssvcMetrics();
  HttpssvcMetrics(HttpssvcMetrics&) = delete;
  HttpssvcMetrics(HttpssvcMetrics&&) = delete;

  // May be called many times.
  void SaveForAddressQuery(base::TimeDelta resolve_time,
                           enum HttpssvcDnsRcode rcode);

  // Save the fact that the non-integrity queries failed. Prevents metrics from
  // being recorded.
  void SaveAddressQueryFailure();

  // Must only be called once.
  void SaveForIntegrity(enum HttpssvcDnsRcode rcode,
                        const std::vector<bool>& condensed_records,
                        base::TimeDelta integrity_resolve_time);
  void SaveForHttps(enum HttpssvcDnsRcode rcode,
                    const std::vector<bool>& condensed_records,
                    base::TimeDelta https_resolve_time);

 private:
  enum class RecordType { kIntegrity, kHttps };

  std::string BuildMetricName(RecordType type,
                              base::StringPiece leaf_name) const;

  // Records all the aggregated metrics to UMA.
  void RecordMetrics();
  void RecordCommonMetrics();
  void RecordExpectIntactMetrics();
  void RecordExpectNoerrorMetrics();

  const bool secure_;
  const bool expect_intact_;
  // RecordIntegrityMetrics() will do nothing when |disqualified_| is true.
  bool disqualified_ = false;
  bool already_recorded_ = false;
  absl::optional<enum HttpssvcDnsRcode> rcode_integrity_;
  absl::optional<enum HttpssvcDnsRcode> rcode_https_;
  size_t num_integrity_records_ = 0;
  size_t num_https_records_ = 0;
  absl::optional<bool> is_integrity_intact_;
  absl::optional<bool> is_https_parsable_;
  // We never make multiple INTEGRITY or HTTPS queries per DnsTask, so we only
  // need one TimeDelta for each qtype.
  absl::optional<base::TimeDelta> integrity_resolve_time_;
  absl::optional<base::TimeDelta> https_resolve_time_;
  std::vector<base::TimeDelta> address_resolve_times_;
};

}  // namespace net

#endif  // NET_DNS_HTTPSSVC_METRICS_H_
