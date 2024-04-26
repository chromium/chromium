// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HTTPSSVC_METRICS_H_
#define NET_DNS_HTTPSSVC_METRICS_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "net/base/net_export.h"

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

// Translate an RCODE value to the |HttpssvcDnsRcode| enum, which is used for
// HTTPSSVC experimentation. The goal is to keep these values in a small,
// contiguous range in order to satisfy the UMA enumeration function's
// requirements. This function never returns |kTimedOut| |kUnrecognizedRcode|,
// or |kMissingDnsResponse|.
enum HttpssvcDnsRcode TranslateDnsRcodeForHttpssvcExperiment(uint8_t rcode);

// Tool for aggregating HTTPS RR metrics. Accumulates metrics via the Save*
// methods. Records metrics to UMA on destruction.
// TODO(crbug.com/40239736): Rework this class once we've finished with
// HTTPS-related rollouts and have decided what metrics we want to keep
// permanently.
class NET_EXPORT_PRIVATE HttpssvcMetrics {
 public:
  explicit HttpssvcMetrics(bool secure);
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
  void SaveForHttps(enum HttpssvcDnsRcode rcode,
                    const std::vector<bool>& condensed_records,
                    base::TimeDelta https_resolve_time);

 private:
  std::string BuildMetricName(std::string_view leaf_name) const;

  // Records all the aggregated metrics to UMA.
  void RecordMetrics();

  const bool secure_;
  // RecordIntegrityMetrics() will do nothing when |disqualified_| is true.
  bool disqualified_ = false;
  bool already_recorded_ = false;
  std::optional<enum HttpssvcDnsRcode> rcode_https_;
  size_t num_https_records_ = 0;
  std::optional<bool> is_https_parsable_;
  // We never make multiple HTTPS queries per DnsTask, so we only need
  // one TimeDelta for the HTTPS query.
  std::optional<base::TimeDelta> https_resolve_time_;
  std::vector<base::TimeDelta> address_resolve_times_;
};

}  // namespace net

#endif  // NET_DNS_HTTPSSVC_METRICS_H_
