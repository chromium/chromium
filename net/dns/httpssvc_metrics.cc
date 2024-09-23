// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/httpssvc_metrics.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/features.h"
#include "net/dns/dns_util.h"
#include "net/dns/public/dns_protocol.h"

namespace net {

enum HttpssvcDnsRcode TranslateDnsRcodeForHttpssvcExperiment(uint8_t rcode) {
  switch (rcode) {
    case dns_protocol::kRcodeNOERROR:
      return HttpssvcDnsRcode::kNoError;
    case dns_protocol::kRcodeFORMERR:
      return HttpssvcDnsRcode::kFormErr;
    case dns_protocol::kRcodeSERVFAIL:
      return HttpssvcDnsRcode::kServFail;
    case dns_protocol::kRcodeNXDOMAIN:
      return HttpssvcDnsRcode::kNxDomain;
    case dns_protocol::kRcodeNOTIMP:
      return HttpssvcDnsRcode::kNotImp;
    case dns_protocol::kRcodeREFUSED:
      return HttpssvcDnsRcode::kRefused;
    default:
      return HttpssvcDnsRcode::kUnrecognizedRcode;
  }
  NOTREACHED_IN_MIGRATION();
}

HttpssvcMetrics::HttpssvcMetrics(bool secure) : secure_(secure) {}

HttpssvcMetrics::~HttpssvcMetrics() {
  RecordMetrics();
}

void HttpssvcMetrics::SaveForAddressQuery(base::TimeDelta resolve_time,
                                          enum HttpssvcDnsRcode rcode) {
  address_resolve_times_.push_back(resolve_time);

  if (rcode != HttpssvcDnsRcode::kNoError)
    disqualified_ = true;
}

void HttpssvcMetrics::SaveAddressQueryFailure() {
  disqualified_ = true;
}

void HttpssvcMetrics::SaveForHttps(enum HttpssvcDnsRcode rcode,
                                   const std::vector<bool>& condensed_records,
                                   base::TimeDelta https_resolve_time) {
  DCHECK(!rcode_https_.has_value());
  rcode_https_ = rcode;

  num_https_records_ = condensed_records.size();

  // We only record one "parsable" sample per HTTPS query. In case multiple
  // matching records are present in the response, we combine their parsable
  // values with logical AND.
  const bool parsable = !base::Contains(condensed_records, false);

  DCHECK(!is_https_parsable_.has_value());
  is_https_parsable_ = parsable;

  DCHECK(!https_resolve_time_.has_value());
  https_resolve_time_ = https_resolve_time;
}

std::string HttpssvcMetrics::BuildMetricName(std::string_view leaf_name) const {
  std::string_view type_str = "RecordHttps";
  std::string_view secure = secure_ ? "Secure" : "Insecure";
  // This part is just a legacy from old experiments but now meaningless.
  std::string_view expectation = "ExpectNoerror";

  // Example metric name:
  // Net.DNS.HTTPSSVC.RecordHttps.Secure.ExpectNoerror.DnsRcode
  // TODO(crbug.com/40239736): Simplify the metric names.
  return base::JoinString(
      {"Net.DNS.HTTPSSVC", type_str, secure, expectation, leaf_name}, ".");
}

void HttpssvcMetrics::RecordMetrics() {
  DCHECK(!already_recorded_);
  already_recorded_ = true;

  // We really have no metrics to record without an HTTPS query resolve time and
  // `address_resolve_times_`. If this HttpssvcMetrics is in an inconsistent
  // state, disqualify any metrics from being recorded.
  if (!https_resolve_time_.has_value() || address_resolve_times_.empty()) {
    disqualified_ = true;
  }
  if (disqualified_)
    return;

  base::UmaHistogramMediumTimes(BuildMetricName("ResolveTimeExperimental"),
                                *https_resolve_time_);

  // Record the address resolve times.
  const std::string kMetricResolveTimeAddressRecord =
      BuildMetricName("ResolveTimeAddress");
  for (base::TimeDelta resolve_time_other : address_resolve_times_) {
    base::UmaHistogramMediumTimes(kMetricResolveTimeAddressRecord,
                                  resolve_time_other);
  }

  // ResolveTimeRatio is the HTTPS query resolve time divided by the slower of
  // the A or AAAA resolve times. Arbitrarily choosing precision at two decimal
  // places.
  std::vector<base::TimeDelta>::iterator slowest_address_resolve =
      std::max_element(address_resolve_times_.begin(),
                       address_resolve_times_.end());
  CHECK(slowest_address_resolve != address_resolve_times_.end(),
        base::NotFatalUntil::M130);

  // It's possible to get here with a zero resolve time in tests.  Avoid
  // divide-by-zero below by returning early; this data point is invalid anyway.
  if (slowest_address_resolve->is_zero())
    return;

  // Compute a percentage showing how much larger the HTTPS query resolve time
  // was compared to the slowest A or AAAA query.
  //
  // Computation happens on TimeDelta objects, which use CheckedNumeric. This
  // will crash if the system clock leaps forward several hundred millennia
  // (numeric_limits<int64_t>::max() microseconds ~= 292,000 years).
  //
  // Then scale the value of the percent by dividing by `kPercentScale`. Sample
  // values are bounded between 1 and 20. A recorded sample of 10 means that the
  // HTTPS query resolve time took 100% of the slower A/AAAA resolve time. A
  // sample of 20 means that the HTTPS query resolve time was 200% relative to
  // the A/AAAA resolve time, twice as long.
  constexpr int64_t kMaxRatio = 20;
  constexpr int64_t kPercentScale = 10;
  const int64_t resolve_time_percent = base::ClampFloor<int64_t>(
      *https_resolve_time_ / *slowest_address_resolve * 100);
  base::UmaHistogramExactLinear(BuildMetricName("ResolveTimeRatio"),
                                resolve_time_percent / kPercentScale,
                                kMaxRatio);

  if (num_https_records_ > 0) {
    DCHECK(rcode_https_.has_value());
    if (*rcode_https_ == HttpssvcDnsRcode::kNoError) {
      base::UmaHistogramBoolean(BuildMetricName("Parsable"),
                                is_https_parsable_.value_or(false));
    } else {
      // Record boolean indicating whether we received an HTTPS record and
      // an error simultaneously.
      base::UmaHistogramBoolean(BuildMetricName("RecordWithError"), true);
    }
  }

  base::UmaHistogramEnumeration(BuildMetricName("DnsRcode"), *rcode_https_);
}

}  // namespace net
