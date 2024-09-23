// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/httpssvc_metrics.h"

#include <optional>
#include <string>
#include <string_view>
#include <tuple>

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// Base for testing the metrics collection code in |HttpssvcMetrics|.
class HttpssvcMetricsTest : public ::testing::TestWithParam<bool> {
 public:
  void SetUp() override { secure_ = GetParam(); }

  std::string BuildMetricNamePrefix() const {
    return base::StrCat({"Net.DNS.HTTPSSVC.RecordHttps.",
                         secure_ ? "Secure." : "Insecure.", "ExpectNoerror."});
  }

  template <typename T>
  void ExpectSample(std::string_view name, std::optional<T> sample) const {
    if (sample)
      histo().ExpectUniqueSample(name, *sample, 1);
    else
      histo().ExpectTotalCount(name, 0);
  }

  void ExpectSample(std::string_view name,
                    std::optional<base::TimeDelta> sample) const {
    std::optional<int64_t> sample_ms;
    if (sample)
      sample_ms = {sample->InMilliseconds()};
    ExpectSample<int64_t>(name, sample_ms);
  }

  void VerifyAddressResolveTimeMetric(
      std::optional<base::TimeDelta> expect_noerror_time = std::nullopt) {
    const std::string kExpectNoerror =
        base::StrCat({BuildMetricNamePrefix(), "ResolveTimeAddress"});

    ExpectSample(kExpectNoerror, expect_noerror_time);
  }

  void VerifyHttpsMetricsForExpectNoerror(
      std::optional<HttpssvcDnsRcode> rcode = std::nullopt,
      std::optional<bool> parsable = std::nullopt,
      std::optional<bool> record_with_error = std::nullopt,
      std::optional<base::TimeDelta> resolve_time_https = std::nullopt,
      std::optional<int> resolve_time_ratio = std::nullopt) const {
    const std::string kPrefix = BuildMetricNamePrefix();
    const std::string kMetricDnsRcode = base::StrCat({kPrefix, "DnsRcode"});
    const std::string kMetricParsable = base::StrCat({kPrefix, "Parsable"});
    const std::string kMetricRecordWithError =
        base::StrCat({kPrefix, "RecordWithError"});
    const std::string kMetricResolveTimeExperimental =
        base::StrCat({kPrefix, "ResolveTimeExperimental"});
    const std::string kMetricResolveTimeRatio =
        base::StrCat({kPrefix, "ResolveTimeRatio"});

    ExpectSample(kMetricDnsRcode, rcode);
    ExpectSample(kMetricParsable, parsable);
    ExpectSample(kMetricRecordWithError, record_with_error);
    ExpectSample(kMetricResolveTimeExperimental, resolve_time_https);
    ExpectSample(kMetricResolveTimeRatio, resolve_time_ratio);
  }

  const base::HistogramTester& histo() const { return histogram_; }

 protected:
  bool secure_;

 private:
  base::HistogramTester histogram_;
};

INSTANTIATE_TEST_SUITE_P(HttpssvcMetricsTestSimple,
                         HttpssvcMetricsTest,
                         testing::Bool()  // Querying over DoH or Do53.
);

// Only record metrics for a non-HTTPS query.
TEST_P(HttpssvcMetricsTest, AddressAndExperimentalMissing) {
  const base::TimeDelta kResolveTime = base::Milliseconds(10);
  auto metrics = std::make_optional<HttpssvcMetrics>(secure_);
  metrics->SaveForAddressQuery(kResolveTime, HttpssvcDnsRcode::kNoError);
  metrics.reset();  // Record the metrics to UMA.

  VerifyAddressResolveTimeMetric();
  VerifyHttpsMetricsForExpectNoerror();
}

TEST_P(HttpssvcMetricsTest, AddressAndHttpsParsable) {
  const base::TimeDelta kResolveTime = base::Milliseconds(10);
  const base::TimeDelta kResolveTimeHttps = base::Milliseconds(15);
  auto metrics = std::make_optional<HttpssvcMetrics>(secure_);
  metrics->SaveForHttps(HttpssvcDnsRcode::kNoError, {true}, kResolveTimeHttps);
  metrics->SaveForAddressQuery(kResolveTime, HttpssvcDnsRcode::kNoError);
  metrics.reset();  // Record the metrics to UMA.

  VerifyAddressResolveTimeMetric({kResolveTime} /* expect_noerror_time */);
  VerifyHttpsMetricsForExpectNoerror(
      {HttpssvcDnsRcode::kNoError} /* rcode */, {true} /* parsable */,
      std::nullopt /* record_with_error */,
      {kResolveTimeHttps} /* resolve_time_https */,
      {15} /* resolve_time_ratio */);
}

// This test simulates an HTTPS response that includes no HTTPS records,
// but does have an error value for the RCODE.
TEST_P(HttpssvcMetricsTest, AddressAndHttpsMissingWithRcode) {
  const base::TimeDelta kResolveTime = base::Milliseconds(10);
  const base::TimeDelta kResolveTimeHttps = base::Milliseconds(15);

  auto metrics = std::make_optional<HttpssvcMetrics>(secure_);
  metrics->SaveForHttps(HttpssvcDnsRcode::kNxDomain, {}, kResolveTimeHttps);
  metrics->SaveForAddressQuery(kResolveTime, HttpssvcDnsRcode::kNoError);
  metrics.reset();  // Record the metrics to UMA.

  VerifyAddressResolveTimeMetric({kResolveTime} /* expect_noerror_time */);
  VerifyHttpsMetricsForExpectNoerror(
      {HttpssvcDnsRcode::kNxDomain} /* rcode */, std::nullopt /* parsable */,
      std::nullopt /* record_with_error */,
      {kResolveTimeHttps} /* resolve_time_https */,
      {15} /* resolve_time_ratio */);
}

// This test simulates an HTTPS response that includes a parsable HTTPS
// record, but also has an error RCODE.
TEST_P(HttpssvcMetricsTest, AddressAndHttpsParsableWithRcode) {
  const base::TimeDelta kResolveTime = base::Milliseconds(10);
  const base::TimeDelta kResolveTimeHttps = base::Milliseconds(15);

  auto metrics = std::make_optional<HttpssvcMetrics>(secure_);
  metrics->SaveForHttps(HttpssvcDnsRcode::kNxDomain, {true}, kResolveTimeHttps);
  metrics->SaveForAddressQuery(kResolveTime, HttpssvcDnsRcode::kNoError);
  metrics.reset();  // Record the metrics to UMA.

  VerifyAddressResolveTimeMetric({kResolveTime} /* expect_noerror_time */);
  VerifyHttpsMetricsForExpectNoerror(
      {HttpssvcDnsRcode::kNxDomain} /* rcode */,
      // "parsable" metric is omitted because the RCODE is not NOERROR.
      std::nullopt /* parsable */, {true} /* record_with_error */,
      {kResolveTimeHttps} /* resolve_time_https */,
      {15} /* resolve_time_ratio */);
}

// This test simulates an HTTPS response that includes a mangled HTTPS
// record *and* has an error RCODE.
TEST_P(HttpssvcMetricsTest, AddressAndHttpsMangledWithRcode) {
  const base::TimeDelta kResolveTime = base::Milliseconds(10);
  const base::TimeDelta kResolveTimeHttps = base::Milliseconds(15);
  auto metrics = std::make_optional<HttpssvcMetrics>(secure_);
  metrics->SaveForHttps(HttpssvcDnsRcode::kNxDomain, {false},
                        kResolveTimeHttps);
  metrics->SaveForAddressQuery(kResolveTime, HttpssvcDnsRcode::kNoError);
  metrics.reset();  // Record the metrics to UMA.

  VerifyAddressResolveTimeMetric({kResolveTime} /* expect_noerror_time */);
  VerifyHttpsMetricsForExpectNoerror(
      {HttpssvcDnsRcode::kNxDomain} /* rcode */,
      // "parsable" metric is omitted because the RCODE is not NOERROR.
      std::nullopt /* parsable */, {true} /* record_with_error */,
      {kResolveTimeHttps} /* resolve_time_https */,
      {15} /* resolve_time_ratio */);
}

// This test simulates successful address queries and an HTTPS query that
// timed out.
TEST_P(HttpssvcMetricsTest, AddressAndHttpsTimedOut) {
  const base::TimeDelta kResolveTime = base::Milliseconds(10);
  const base::TimeDelta kResolveTimeHttps = base::Milliseconds(15);
  auto metrics = std::make_optional<HttpssvcMetrics>(secure_);
  metrics->SaveForHttps(HttpssvcDnsRcode::kTimedOut, {}, kResolveTimeHttps);
  metrics->SaveForAddressQuery(kResolveTime, HttpssvcDnsRcode::kNoError);
  metrics.reset();  // Record the metrics to UMA.

  VerifyAddressResolveTimeMetric({kResolveTime} /* expect_noerror_time */);
  VerifyHttpsMetricsForExpectNoerror(
      {HttpssvcDnsRcode::kTimedOut} /* rcode */,
      // "parsable" metric is omitted because the RCODE is not NOERROR.
      std::nullopt /* parsable */, std::nullopt /* record_with_error */,
      {kResolveTimeHttps} /* resolve_time_https */,
      {15} /* resolve_time_ratio */);
}

}  // namespace net
