// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/httpssvc_metrics.h"

#include "base/feature_list.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
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
  NOTREACHED();
}

HttpssvcExperimentDomainCache::HttpssvcExperimentDomainCache() = default;
HttpssvcExperimentDomainCache::~HttpssvcExperimentDomainCache() = default;

bool HttpssvcExperimentDomainCache::ListContainsDomain(
    const std::string& domain_list,
    base::StringPiece domain,
    base::Optional<base::flat_set<std::string>>& in_out_cached_list) {
  if (!in_out_cached_list) {
    in_out_cached_list = base::SplitString(
        domain_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  }
  return in_out_cached_list->find(domain) != in_out_cached_list->end();
}

bool HttpssvcExperimentDomainCache::IsExperimental(base::StringPiece domain) {
  if (!base::FeatureList::IsEnabled(features::kDnsHttpssvc))
    return false;
  return ListContainsDomain(features::kDnsHttpssvcExperimentDomains.Get(),
                            domain, experimental_list_);
}

bool HttpssvcExperimentDomainCache::IsControl(base::StringPiece domain) {
  if (!base::FeatureList::IsEnabled(features::kDnsHttpssvc))
    return false;
  if (features::kDnsHttpssvcControlDomainWildcard.Get())
    return !IsExperimental(domain);
  return ListContainsDomain(features::kDnsHttpssvcControlDomains.Get(), domain,
                            control_list_);
}

HttpssvcMetrics::HttpssvcMetrics(bool expect_intact)
    : expect_intact_(expect_intact) {}

HttpssvcMetrics::~HttpssvcMetrics() {
  RecordIntegrityMetrics();
}

void HttpssvcMetrics::SaveForNonIntegrity(
    base::Optional<std::string> new_doh_provider_id,
    base::TimeDelta resolve_time,
    enum HttpssvcDnsRcode rcode) {
  set_doh_provider_id(new_doh_provider_id);

  non_integrity_resolve_times_.push_back(resolve_time);

  if (rcode != HttpssvcDnsRcode::kNoError)
    disqualified_ = true;
}

void HttpssvcMetrics::SaveNonIntegrityFailure() {
  disqualified_ = true;
}

void HttpssvcMetrics::SaveForIntegrity(
    base::Optional<std::string> new_doh_provider_id,
    enum HttpssvcDnsRcode rcode_integrity,
    const std::vector<bool>& condensed_records,
    base::TimeDelta integrity_resolve_time) {
  DCHECK(!rcode_integrity_.has_value());
  set_doh_provider_id(new_doh_provider_id);

  rcode_integrity_ = rcode_integrity;

  num_integrity_records_ = condensed_records.size();

  // We only record one "Integrity" sample per INTEGRITY query. In case
  // multiple matching records are in present in the response, we
  // combine their intactness values with logical AND.
  const bool intact =
      std::all_of(condensed_records.cbegin(), condensed_records.cend(),
                  [](bool b) { return b; });

  DCHECK(!is_integrity_intact_.has_value());
  is_integrity_intact_ = intact;

  DCHECK(!integrity_resolve_time_.has_value());
  integrity_resolve_time_ = integrity_resolve_time;
}

void HttpssvcMetrics::SaveForHttps(base::Optional<std::string> doh_provider_id,
                                   enum HttpssvcDnsRcode rcode,
                                   base::TimeDelta https_resolve_time) {
  // TODO(crbug.com/1138620): Implement.
}

void HttpssvcMetrics::set_doh_provider_id(
    base::Optional<std::string> new_doh_provider_id) {
  // "Other" never gets updated.
  if (doh_provider_id_.has_value() && *doh_provider_id_ == "Other")
    return;

  // If provider IDs mismatch, downgrade the new provider ID to "Other".
  if ((doh_provider_id_.has_value() && !new_doh_provider_id.has_value()) ||
      (doh_provider_id_.has_value() && new_doh_provider_id.has_value() &&
       *doh_provider_id_ != *new_doh_provider_id)) {
    new_doh_provider_id = "Other";
  }

  doh_provider_id_ = new_doh_provider_id;
}

std::string HttpssvcMetrics::BuildMetricName(
    base::StringPiece leaf_name) const {
  // Build shared pieces of the metric names.
  const base::StringPiece expectation =
      expect_intact_ ? "ExpectIntact" : "ExpectNoerror";
  const std::string provider_id = doh_provider_id_.value_or("Other");

  // Example INTEGRITY metric name:
  // Net.DNS.HTTPSSVC.RecordIntegrity.CleanBrowsingAdult.ExpectIntact.DnsRcode
  return base::JoinString({"Net.DNS.HTTPSSVC.RecordIntegrity",
                           provider_id.c_str(), expectation, leaf_name},
                          ".");
}

void HttpssvcMetrics::RecordIntegrityMetrics() {
  // The HTTPSSVC experiment and its feature param indicating INTEGRITY must
  // both be enabled.
  DCHECK(base::FeatureList::IsEnabled(features::kDnsHttpssvc));
  DCHECK(features::kDnsHttpssvcUseIntegrity.Get() ||
         features::kDnsHttpssvcUseHttpssvc.Get());

  DCHECK(!already_recorded_);
  already_recorded_ = true;

  // We really have no metrics to record without |integrity_resolve_time_| and
  // |non_integrity_resolve_times_|. If this HttpssvcMetrics is in an
  // inconsistent state, disqualify any metrics from being recorded.
  if (!integrity_resolve_time_.has_value() ||
      non_integrity_resolve_times_.empty()) {
    disqualified_ = true;
  }
  if (disqualified_)
    return;

  // Record the metrics that the "ExpectIntact" and "ExpectNoerror" branches
  // have in common.
  RecordIntegrityCommonMetrics();

  if (expect_intact_) {
    // Record metrics that are unique to the "ExpectIntact" branch.
    RecordIntegrityExpectIntactMetrics();
  } else {
    // Record metrics that are unique to the "ExpectNoerror" branch.
    RecordIntegrityExpectNoerrorMetrics();
  }
}

void HttpssvcMetrics::RecordIntegrityCommonMetrics() {
  base::UmaHistogramMediumTimes(BuildMetricName("ResolveTimeIntegrityRecord"),
                                *integrity_resolve_time_);

  const std::string kMetricResolveTimeNonIntegrityRecord =
      BuildMetricName("ResolveTimeNonIntegrityRecord");
  for (base::TimeDelta resolve_time_other : non_integrity_resolve_times_) {
    base::UmaHistogramMediumTimes(kMetricResolveTimeNonIntegrityRecord,
                                  resolve_time_other);
  }

  // ResolveTimeRatio is the INTEGRITY resolve time divided by the slower of the
  // A or AAAA resolve times. Arbitrarily choosing precision at two decimal
  // places.
  std::vector<base::TimeDelta>::iterator slowest_non_integrity_resolve =
      std::max_element(non_integrity_resolve_times_.begin(),
                       non_integrity_resolve_times_.end());
  DCHECK(slowest_non_integrity_resolve != non_integrity_resolve_times_.end());

  // It's possible to get here with a zero resolve time in tests.  Avoid
  // divide-by-zero below by returning early; this data point is invalid anyway.
  if (slowest_non_integrity_resolve->is_zero())
    return;

  // Compute a percentage showing how much larger the INTEGRITY resolve time was
  // compared to the slowest A or AAAA query.
  //
  // Computation happens on TimeDelta objects, which use CheckedNumeric. This
  // will crash if the system clock leaps forward several hundred millennia
  // (numeric_limits<int64_t>::max() microseconds ~= 292,000 years).
  const int64_t resolve_time_percent = base::ClampFloor<int64_t>(
      *integrity_resolve_time_ / *slowest_non_integrity_resolve * 100);

  // Scale the value of |resolve_time_percent| by dividing by |kPercentScale|.
  // Sample values are bounded between 1 and 20. A recorded sample of 10 means
  // that the INTEGRITY resolve time took 100% of the slower A/AAAA resolve
  // time. A sample of 20 means that the INTEGRITY resolve time was 200%
  // relative to the A/AAAA resolve time, twice as long.
  constexpr int64_t kMaxRatio = 20;
  constexpr int64_t kPercentScale = 10;
  base::UmaHistogramExactLinear(BuildMetricName("ResolveTimeRatio"),
                                resolve_time_percent / kPercentScale,
                                kMaxRatio);
}

void HttpssvcMetrics::RecordIntegrityExpectIntactMetrics() {
  // Without |rocde_integrity_|, we can't make progress on any of these metrics.
  DCHECK(rcode_integrity_.has_value());

  // The ExpectIntact variant of the "DnsRcode" metric is only recorded when no
  // records are received.
  if (num_integrity_records_ == 0) {
    base::UmaHistogramEnumeration(BuildMetricName("DnsRcode"),
                                  *rcode_integrity_);
  }
  if (num_integrity_records_ > 0) {
    if (*rcode_integrity_ == HttpssvcDnsRcode::kNoError) {
      base::UmaHistogramBoolean(BuildMetricName("Integrity"),
                                is_integrity_intact_.value_or(false));
    } else if (*rcode_integrity_ != HttpssvcDnsRcode::kNoError) {
      // Record boolean indicating whether we received an INTEGRITY record and
      // an error simultaneously.
      base::UmaHistogramBoolean(BuildMetricName("RecordWithError"), true);
    }
  }
}

void HttpssvcMetrics::RecordIntegrityExpectNoerrorMetrics() {
  if (rcode_integrity_.has_value()) {
    base::UmaHistogramEnumeration(BuildMetricName("DnsRcode"),
                                  *rcode_integrity_);
  }
  if (num_integrity_records_ > 0) {
    base::UmaHistogramBoolean(BuildMetricName("RecordReceived"), true);
  }
}

}  // namespace net
