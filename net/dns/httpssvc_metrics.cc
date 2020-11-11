// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/httpssvc_metrics.h"

#include "base/feature_list.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
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
  RecordMetrics();
}

void HttpssvcMetrics::SaveForAddressQuery(
    base::Optional<std::string> new_doh_provider_id,
    base::TimeDelta resolve_time,
    enum HttpssvcDnsRcode rcode) {
  set_doh_provider_id(new_doh_provider_id);

  address_resolve_times_.push_back(resolve_time);

  if (rcode != HttpssvcDnsRcode::kNoError)
    disqualified_ = true;
}

void HttpssvcMetrics::SaveAddressQueryFailure() {
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

  // We only record one "Integrity" sample per INTEGRITY query. In case multiple
  // matching records are present in the response, we combine their intactness
  // values with logical AND.
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
                                   const std::vector<bool>& condensed_records,
                                   base::TimeDelta https_resolve_time) {
  DCHECK(!rcode_https_.has_value());
  set_doh_provider_id(doh_provider_id);

  rcode_https_ = rcode;

  num_https_records_ = condensed_records.size();

  // We only record one "parsable" sample per HTTPS query. In case multiple
  // matching records are present in the response, we combine their parsable
  // values with logical AND.
  const bool parsable =
      std::all_of(condensed_records.cbegin(), condensed_records.cend(),
                  [](bool b) { return b; });

  DCHECK(!is_https_parsable_.has_value());
  is_https_parsable_ = parsable;

  DCHECK(!https_resolve_time_.has_value());
  https_resolve_time_ = https_resolve_time;
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
    RecordType type,
    base::StringPiece leaf_name) const {
  // Build shared pieces of the metric names.
  base::StringPiece type_str;
  switch (type) {
    case RecordType::kIntegrity:
      type_str = "RecordIntegrity";
      break;
    case RecordType::kHttps:
      type_str = "RecordHttps";
      break;
  }
  const base::StringPiece expectation =
      expect_intact_ ? "ExpectIntact" : "ExpectNoerror";
  const std::string provider_id = doh_provider_id_.value_or("Other");

  // Example INTEGRITY metric name:
  // Net.DNS.HTTPSSVC.RecordIntegrity.CleanBrowsingAdult.ExpectIntact.DnsRcode
  return base::JoinString({"Net.DNS.HTTPSSVC", type_str, provider_id.c_str(),
                           expectation, leaf_name},
                          ".");
}

void HttpssvcMetrics::RecordMetrics() {
  // The HTTPSSVC experiment and its feature param indicating INTEGRITY must
  // both be enabled.
  DCHECK(base::FeatureList::IsEnabled(features::kDnsHttpssvc));
  DCHECK(features::kDnsHttpssvcUseIntegrity.Get() ||
         features::kDnsHttpssvcUseHttpssvc.Get());

  DCHECK(!already_recorded_);
  already_recorded_ = true;

  // We really have no metrics to record without an experimental query resolve
  // time and `address_resolve_times_`. If this HttpssvcMetrics is in an
  // inconsistent state, disqualify any metrics from being recorded.
  if ((!integrity_resolve_time_.has_value() &&
       !https_resolve_time_.has_value()) ||
      address_resolve_times_.empty()) {
    disqualified_ = true;
  }
  if (disqualified_)
    return;

  // Record the metrics that the "ExpectIntact" and "ExpectNoerror" branches
  // have in common.
  RecordCommonMetrics();

  if (expect_intact_) {
    // Record metrics that are unique to the "ExpectIntact" branch.
    RecordExpectIntactMetrics();
  } else {
    // Record metrics that are unique to the "ExpectNoerror" branch.
    RecordExpectNoerrorMetrics();
  }
}

void HttpssvcMetrics::RecordCommonMetrics() {
  DCHECK(integrity_resolve_time_.has_value() ||
         https_resolve_time_.has_value());
  if (integrity_resolve_time_.has_value()) {
    base::UmaHistogramMediumTimes(
        BuildMetricName(RecordType::kIntegrity, "ResolveTimeIntegrityRecord"),
        *integrity_resolve_time_);
  }
  if (https_resolve_time_.has_value()) {
    base::UmaHistogramMediumTimes(
        BuildMetricName(RecordType::kHttps, "ResolveTimeHttpsRecord"),
        *https_resolve_time_);
  }

  DCHECK(!address_resolve_times_.empty());
  // Not specific to INTEGRITY or HTTPS, but for the sake of picking one for the
  // metric name and only recording the time once, always record the address
  // resolve times under `kIntegrity`.
  const std::string kMetricResolveTimeAddressRecord =
      BuildMetricName(RecordType::kIntegrity, "ResolveTimeNonIntegrityRecord");
  for (base::TimeDelta resolve_time_other : address_resolve_times_) {
    base::UmaHistogramMediumTimes(kMetricResolveTimeAddressRecord,
                                  resolve_time_other);
  }

  // ResolveTimeRatio is the experimental query resolve time divided by the
  // slower of the A or AAAA resolve times. Arbitrarily choosing precision at
  // two decimal places.
  std::vector<base::TimeDelta>::iterator slowest_address_resolve =
      std::max_element(address_resolve_times_.begin(),
                       address_resolve_times_.end());
  DCHECK(slowest_address_resolve != address_resolve_times_.end());

  // It's possible to get here with a zero resolve time in tests.  Avoid
  // divide-by-zero below by returning early; this data point is invalid anyway.
  if (slowest_address_resolve->is_zero())
    return;

  // Compute a percentage showing how much larger the experimental query resolve
  // time was compared to the slowest A or AAAA query.
  //
  // Computation happens on TimeDelta objects, which use CheckedNumeric. This
  // will crash if the system clock leaps forward several hundred millennia
  // (numeric_limits<int64_t>::max() microseconds ~= 292,000 years).
  //
  // Then scale the value of the percent by dividing by `kPercentScale`. Sample
  // values are bounded between 1 and 20. A recorded sample of 10 means that the
  // experimental query resolve time took 100% of the slower A/AAAA resolve
  // time. A sample of 20 means that the experimental query resolve time was
  // 200% relative to the A/AAAA resolve time, twice as long.
  constexpr int64_t kMaxRatio = 20;
  constexpr int64_t kPercentScale = 10;
  if (integrity_resolve_time_.has_value()) {
    const int64_t resolve_time_percent = base::ClampFloor<int64_t>(
        *integrity_resolve_time_ / *slowest_address_resolve * 100);
    base::UmaHistogramExactLinear(
        BuildMetricName(RecordType::kIntegrity, "ResolveTimeRatio"),
        resolve_time_percent / kPercentScale, kMaxRatio);
  }
  if (https_resolve_time_.has_value()) {
    const int64_t resolve_time_percent = base::ClampFloor<int64_t>(
        *https_resolve_time_ / *slowest_address_resolve * 100);
    base::UmaHistogramExactLinear(
        BuildMetricName(RecordType::kHttps, "ResolveTimeRatio"),
        resolve_time_percent / kPercentScale, kMaxRatio);
  }
}

void HttpssvcMetrics::RecordExpectIntactMetrics() {
  // Without an experimental query rcode, we can't make progress on any of these
  // metrics.
  DCHECK(rcode_integrity_.has_value() || rcode_https_.has_value());

  // The ExpectIntact variant of the "DnsRcode" metric is only recorded when no
  // records are received.
  if (num_integrity_records_ == 0 && rcode_integrity_.has_value()) {
    base::UmaHistogramEnumeration(
        BuildMetricName(RecordType::kIntegrity, "DnsRcode"), *rcode_integrity_);
  }
  if (num_https_records_ == 0 && rcode_https_.has_value()) {
    base::UmaHistogramEnumeration(
        BuildMetricName(RecordType::kHttps, "DnsRcode"), *rcode_https_);
  }

  if (num_integrity_records_ > 0) {
    DCHECK(rcode_integrity_.has_value());
    if (*rcode_integrity_ == HttpssvcDnsRcode::kNoError) {
      base::UmaHistogramBoolean(
          BuildMetricName(RecordType::kIntegrity, "Integrity"),
          is_integrity_intact_.value_or(false));
    } else {
      // Record boolean indicating whether we received an INTEGRITY record and
      // an error simultaneously.
      base::UmaHistogramBoolean(
          BuildMetricName(RecordType::kIntegrity, "RecordWithError"), true);
    }
  }
  if (num_https_records_ > 0) {
    DCHECK(rcode_https_.has_value());
    if (*rcode_https_ == HttpssvcDnsRcode::kNoError) {
      base::UmaHistogramBoolean(BuildMetricName(RecordType::kHttps, "Parsable"),
                                is_https_parsable_.value_or(false));
    } else {
      // Record boolean indicating whether we received an HTTPS record and
      // an error simultaneously.
      base::UmaHistogramBoolean(
          BuildMetricName(RecordType::kHttps, "RecordWithError"), true);
    }
  }
}

void HttpssvcMetrics::RecordExpectNoerrorMetrics() {
  if (rcode_integrity_.has_value()) {
    base::UmaHistogramEnumeration(
        BuildMetricName(RecordType::kIntegrity, "DnsRcode"), *rcode_integrity_);
  }
  if (rcode_https_.has_value()) {
    base::UmaHistogramEnumeration(
        BuildMetricName(RecordType::kHttps, "DnsRcode"), *rcode_https_);
  }

  // INTEGRITY only records a simple boolean when an unexpected record is
  // received because it is extremely unlikely to be an actual INTEGRITY record.
  if (num_integrity_records_ > 0) {
    base::UmaHistogramBoolean(
        BuildMetricName(RecordType::kIntegrity, "RecordReceived"), true);
  }

  // HTTPS records received for expect-noerror domains are actual in-the-wild
  // records not specific to Chrome experiments. Record some extra metrics on
  // seen records, but not broken out by DNS provider.
  if (num_https_records_ > 0) {
    if (*rcode_https_ == HttpssvcDnsRcode::kNoError) {
      UMA_HISTOGRAM_BOOLEAN(
          "Net.DNS.HTTPSSVC.RecordHttps.AnyProvider.ExpectNoerror.Parsable",
          is_https_parsable_.value_or(false));
    } else {
      // Record boolean indicating whether we received an HTTPS record and
      // an error simultaneously.
      UMA_HISTOGRAM_BOOLEAN(
          "Net.DNS.HTTPSSVC.RecordHttps.AnyProvider.ExpectNoerror."
          "RecordWithError",
          true);
    }
  }
}

}  // namespace net
