// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/httpssvc_metrics.h"

#include <string>
#include <tuple>

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// int: number of domains
// bool: extra leading comma
// bool: extra trailing comma
using DomainListQuirksTuple = std::tuple<int, bool, bool>;

// bool: DnsHttpssvc feature is enabled
// bool: DnsHttpssvcUseIntegrity feature param
// bool: DnsHttpssvcUseHttpssvc feature param
// bool: DnsHttpssvcControlDomainWildcard feature param
using HttpssvcFeatureTuple = std::tuple<bool, bool, bool, bool>;

// DomainListQuirksTuple: quirks for the experimental domain list.
// DomainListQuirksTuple: quirks for the control domain list.
// HttpssvcFeatureTuple: config for the whole DnsHttpssvc feature.
using ParsingTestParamTuple = std::
    tuple<DomainListQuirksTuple, DomainListQuirksTuple, HttpssvcFeatureTuple>;

// bool: whether we are querying for an experimental domain or a control domain
// HttpssvcFeatureTuple: config for the whole DnsHttpssvc feature.
using MetricsTestParamTuple = std::tuple<bool, HttpssvcFeatureTuple>;

// Create a comma-separated list of |domains| with the given |quirks|.
std::string FlattenDomainList(const std::vector<std::string>& domains,
                              DomainListQuirksTuple quirks) {
  auto [num_domains, leading_comma, trailing_comma] = quirks;

  CHECK_EQ(static_cast<size_t>(num_domains), domains.size());
  std::string flattened = base::JoinString(domains, ",");
  if (leading_comma)
    flattened.insert(flattened.begin(), ',');
  if (trailing_comma)
    flattened.push_back(',');
  return flattened;
}

// Intermediate representation constructed from test parameters.
struct HttpssvcFeatureConfig {
  HttpssvcFeatureConfig() = default;

  explicit HttpssvcFeatureConfig(const HttpssvcFeatureTuple& feature_tuple,
                                 base::StringPiece experiment_domains,
                                 base::StringPiece control_domains)
      : experiment_domains(experiment_domains),
        control_domains(control_domains) {
    std::tie(enabled, use_integrity, use_httpssvc, control_domain_wildcard) =
        feature_tuple;
  }

  void Apply(base::test::ScopedFeatureList* scoped_feature_list) const {
    if (!enabled) {
      scoped_feature_list->InitAndDisableFeature(features::kDnsHttpssvc);
      return;
    }
    auto stringify = [](bool b) -> std::string { return b ? "true" : "false"; };
    scoped_feature_list->InitAndEnableFeatureWithParameters(
        features::kDnsHttpssvc,
        {
            {"DnsHttpssvcUseHttpssvc", stringify(use_httpssvc)},
            {"DnsHttpssvcUseIntegrity", stringify(use_integrity)},
            {"DnsHttpssvcEnableQueryOverInsecure", "false"},
            {"DnsHttpssvcExperimentDomains", experiment_domains},
            {"DnsHttpssvcControlDomains", control_domains},
            {"DnsHttpssvcControlDomainWildcard",
             stringify(control_domain_wildcard)},
        });
  }

  bool enabled = false;
  bool use_integrity = false;
  bool use_httpssvc = false;
  bool control_domain_wildcard = false;
  std::string experiment_domains;
  std::string control_domains;
};

std::vector<std::string> GenerateDomainList(base::StringPiece label, int n) {
  std::vector<std::string> domains;
  for (int i = 0; i < n; i++) {
    domains.push_back(base::StrCat(
        {"domain", base::NumberToString(i), ".", label, ".example"}));
  }
  return domains;
}

// Base for testing domain list parsing functions in
// net::features::dns_httpssvc_experiment.
class HttpssvcDomainParsingTest
    : public ::testing::TestWithParam<ParsingTestParamTuple> {
 public:
  void SetUp() override {
    auto [domain_quirks_experimental, domain_quirks_control, httpssvc_feature] =
        GetParam();

    expected_experiment_domains_ = GenerateDomainList(
        "experiment", std::get<0>(domain_quirks_experimental));
    expected_control_domains_ =
        GenerateDomainList("control", std::get<0>(domain_quirks_control));

    config_ = HttpssvcFeatureConfig(
        httpssvc_feature,
        FlattenDomainList(expected_experiment_domains_,
                          domain_quirks_experimental),
        FlattenDomainList(expected_control_domains_, domain_quirks_control));
    config_.Apply(&scoped_feature_list_);
  }

  const HttpssvcFeatureConfig& config() { return config_; }

 protected:
  // The expected results of parsing the comma-separated domain lists in
  // |experiment_domains| and |control_domains|, respectively.
  std::vector<std::string> expected_experiment_domains_;
  std::vector<std::string> expected_control_domains_;

 private:
  HttpssvcFeatureConfig config_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This instantiation tests the domain list parser against various quirks,
// e.g. leading comma.
INSTANTIATE_TEST_SUITE_P(
    HttpssvcMetricsTestDomainParsing,
    HttpssvcDomainParsingTest,
    testing::Combine(
        // DomainListQuirksTuple for experimental domains. To fight back
        // combinatorial explosion of tests, this tuple is pared down more than
        // the one for control domains. This should not significantly hurt test
        // coverage because |IsExperimentDomain| and |IsControlDomain| rely on a
        // shared helper function.
        testing::Combine(testing::Values(0, 1),
                         testing::Values(false),
                         testing::Values(false)),
        // DomainListQuirksTuple for control domains.
        testing::Combine(testing::Range(0, 3),
                         testing::Bool(),
                         testing::Bool()),
        // HttpssvcFeatureTuple
        testing::Combine(
            testing::Bool() /* DnsHttpssvc feature enabled? */,
            testing::Bool() /* DnsHttpssvcUseIntegrity */,
            testing::Bool() /* DnsHttpssvcUseHttpssvc */,
            testing::Values(false) /* DnsHttpssvcControlDomainWildcard */)));

// Base for testing the metrics collection code in |HttpssvcMetrics|.
class HttpssvcMetricsTest : public ::testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    querying_experimental_ = GetParam();
    config_ = HttpssvcFeatureConfig(
        {true /* enabled */, true /* use_integrity */, true /* use_httpssvc */,
         false /* control_domain_wildcard */},
        "", "");
    config_.Apply(&scoped_feature_list_);
  }

  std::string BuildMetricNamePrefix(base::StringPiece record_type_str,
                                    base::StringPiece expect_str) const {
    return base::StrCat({"Net.DNS.HTTPSSVC.", record_type_str, ".",
                         doh_provider_, ".", expect_str, "."});
  }

  template <typename T>
  void ExpectSample(base::StringPiece name, absl::optional<T> sample) const {
    if (sample)
      histo().ExpectUniqueSample(name, *sample, 1);
    else
      histo().ExpectTotalCount(name, 0);
  }

  void ExpectSample(base::StringPiece name,
                    absl::optional<base::TimeDelta> sample) const {
    absl::optional<int64_t> sample_ms;
    if (sample)
      sample_ms = {sample->InMilliseconds()};
    ExpectSample<int64_t>(name, sample_ms);
  }

  void VerifyAddressResolveTimeMetric(
      absl::optional<base::TimeDelta> expect_intact_time = absl::nullopt,
      absl::optional<base::TimeDelta> expect_noerror_time = absl::nullopt) {
    const std::string kExpectIntact =
        base::StrCat({BuildMetricNamePrefix("RecordIntegrity", "ExpectIntact"),
                      "ResolveTimeNonIntegrityRecord"});
    const std::string kExpectNoerror =
        base::StrCat({BuildMetricNamePrefix("RecordIntegrity", "ExpectNoerror"),
                      "ResolveTimeNonIntegrityRecord"});

    ExpectSample(kExpectIntact, expect_intact_time);
    ExpectSample(kExpectNoerror, expect_noerror_time);
  }

  void VerifyIntegrityMetricsForExpectIntact(
      absl::optional<HttpssvcDnsRcode> rcode,
      absl::optional<bool> integrity,
      absl::optional<bool> record_with_error,
      absl::optional<base::TimeDelta> resolve_time_integrity,
      absl::optional<int> resolve_time_ratio) const {
    const std::string kPrefix =
        BuildMetricNamePrefix("RecordIntegrity", "ExpectIntact");
    const std::string kMetricDnsRcode = base::StrCat({kPrefix, "DnsRcode"});
    const std::string kMetricIntegrity = base::StrCat({kPrefix, "Integrity"});
    const std::string kMetricRecordWithError =
        base::StrCat({kPrefix, "RecordWithError"});
    const std::string kMetricResolveTimeIntegrity =
        base::StrCat({kPrefix, "ResolveTimeIntegrityRecord"});
    const std::string kMetricResolveTimeRatio =
        base::StrCat({kPrefix, "ResolveTimeRatio"});

    ExpectSample(kMetricDnsRcode, rcode);
    ExpectSample(kMetricIntegrity, integrity);
    ExpectSample(kMetricRecordWithError, record_with_error);
    ExpectSample(kMetricResolveTimeIntegrity, resolve_time_integrity);
    ExpectSample(kMetricResolveTimeRatio, resolve_time_ratio);
  }

  void VerifyHttpsMetricsForExpectIntact(
      absl::optional<HttpssvcDnsRcode> rcode = absl::nullopt,
      absl::optional<bool> parsable = absl::nullopt,
      absl::optional<bool> record_with_error = absl::nullopt,
      absl::optional<base::TimeDelta> resolve_time_https = absl::nullopt,
      absl::optional<int> resolve_time_ratio = absl::nullopt) const {
    const std::string kPrefix =
        BuildMetricNamePrefix("RecordHttps", "ExpectIntact");
    const std::string kMetricDnsRcode = base::StrCat({kPrefix, "DnsRcode"});
    const std::string kMetricParsable = base::StrCat({kPrefix, "Parsable"});
    const std::string kMetricRecordWithError =
        base::StrCat({kPrefix, "RecordWithError"});
    const std::string kMetricResolveTimeHttps =
        base::StrCat({kPrefix, "ResolveTimeHttpsRecord"});
    const std::string kMetricResolveTimeRatio =
        base::StrCat({kPrefix, "ResolveTimeRatio"});

    ExpectSample(kMetricDnsRcode, rcode);
    ExpectSample(kMetricParsable, parsable);
    ExpectSample(kMetricRecordWithError, record_with_error);
    ExpectSample(kMetricResolveTimeHttps, resolve_time_https);
    ExpectSample(kMetricResolveTimeRatio, resolve_time_ratio);
  }

  void VerifyIntegrityMetricsForExpectNoerror(
      absl::optional<HttpssvcDnsRcode> rcode,
      absl::optional<int> record_received,
      absl::optional<base::TimeDelta> resolve_time_integrity,
      absl::optional<int> resolve_time_ratio) const {
    const std::string kPrefix =
        BuildMetricNamePrefix("RecordIntegrity", "ExpectNoerror");
    const std::string kMetricDnsRcode = base::StrCat({kPrefix, "DnsRcode"});
    const std::string kMetricRecordReceived =
        base::StrCat({kPrefix, "RecordReceived"});
    const std::string kMetricResolveTimeIntegrity =
        base::StrCat({kPrefix, "ResolveTimeIntegrityRecord"});
    const std::string kMetricResolveTimeRatio =
        base::StrCat({kPrefix, "ResolveTimeRatio"});

    ExpectSample(kMetricDnsRcode, rcode);
    ExpectSample(kMetricRecordReceived, record_received);
    ExpectSample(kMetricResolveTimeIntegrity, resolve_time_integrity);
    ExpectSample(kMetricResolveTimeRatio, resolve_time_ratio);
  }

  void VerifyHttpsMetricsForExpectNoerror(
      absl::optional<HttpssvcDnsRcode> rcode = absl::nullopt,
      absl::optional<bool> parsable = absl::nullopt,
      absl::optional<bool> record_with_error = absl::nullopt,
      absl::optional<base::TimeDelta> resolve_time_https = absl::nullopt,
      absl::optional<int> resolve_time_ratio = absl::nullopt) const {
    const std::string kPrefix =
        BuildMetricNamePrefix("RecordHttps", "ExpectNoerror");
    const std::string kMetricDnsRcode = base::StrCat({kPrefix, "DnsRcode"});
    const std::string kMetricParsable =
        "Net.DNS.HTTPSSVC.RecordHttps.AnyProvider.ExpectNoerror.Parsable";
    const std::string kMetricRecordWithError =
        "Net.DNS.HTTPSSVC.RecordHttps.AnyProvider.ExpectNoerror."
        "RecordWithError";
    const std::string kMetricResolveTimeHttps =
        base::StrCat({kPrefix, "ResolveTimeHttpsRecord"});
    const std::string kMetricResolveTimeRatio =
        base::StrCat({kPrefix, "ResolveTimeRatio"});

    ExpectSample(kMetricDnsRcode, rcode);
    ExpectSample(kMetricParsable, parsable);
    ExpectSample(kMetricRecordWithError, record_with_error);
    ExpectSample(kMetricResolveTimeHttps, resolve_time_https);
    ExpectSample(kMetricResolveTimeRatio, resolve_time_ratio);
  }

  void VerifyIntegrityMetricsForExpectIntact() {
    VerifyIntegrityMetricsForExpectIntact(absl::nullopt, absl::nullopt,
                                          absl::nullopt, absl::nullopt,
                                          absl::nullopt);
  }

  void VerifyIntegrityMetricsForExpectNoerror() {
    VerifyIntegrityMetricsForExpectNoerror(absl::nullopt, absl::nullopt,
                                           absl::nullopt, absl::nullopt);
  }

  const base::HistogramTester& histo() const { return histogram_; }
  const HttpssvcFeatureConfig& config() const { return config_; }

 protected:
  bool querying_experimental_;

 private:
  HttpssvcFeatureConfig config_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_;
  std::string doh_provider_ = "Other";
};

// This instantiation focuses on whether the correct metrics are recorded. The
// domain list parser is already tested against encoding quirks in
// |HttpssvcMetricsTestDomainParsing|, so we fix the quirks at false.
INSTANTIATE_TEST_SUITE_P(HttpssvcMetricsTestSimple,
                         HttpssvcMetricsTest,
                         // Whether we are querying an experimental domain.
                         testing::Bool());

TEST_P(HttpssvcDomainParsingTest, ParseFeatureParamIntegrityDomains) {
  HttpssvcExperimentDomainCache domain_cache;

  const std::string kReservedDomain = "neither.example";
  EXPECT_FALSE(domain_cache.IsExperimental(kReservedDomain));
  EXPECT_EQ(domain_cache.IsControl(kReservedDomain),
            config().enabled && config().control_domain_wildcard);

  // If |config().use_integrity| is true, then we expect all domains in
  // |expected_experiment_domains_| to be experimental (same goes for
  // control domains). Otherwise, no domain should be considered experimental or
  // control.

  if (!config().enabled) {
    // When the HTTPSSVC feature is disabled, no domain should be considered
    // experimental or control.
    for (const std::string& experiment_domain : expected_experiment_domains_) {
      EXPECT_FALSE(domain_cache.IsExperimental(experiment_domain));
      EXPECT_FALSE(domain_cache.IsControl(experiment_domain));
    }
    for (const std::string& control_domain : expected_control_domains_) {
      EXPECT_FALSE(domain_cache.IsExperimental(control_domain));
      EXPECT_FALSE(domain_cache.IsControl(control_domain));
    }
  } else if (config().use_integrity || config().use_httpssvc) {
    for (const std::string& experiment_domain : expected_experiment_domains_) {
      EXPECT_TRUE(domain_cache.IsExperimental(experiment_domain));
      EXPECT_FALSE(domain_cache.IsControl(experiment_domain));
    }
    for (const std::string& control_domain : expected_control_domains_) {
      EXPECT_FALSE(domain_cache.IsExperimental(control_domain));
      EXPECT_TRUE(domain_cache.IsControl(control_domain));
    }
    return;
  }
}

// Only record metrics for a non-integrity query.
TEST_P(HttpssvcMetricsTest, AddressAndExperimentalMissing) {
  const base::TimeDelta kResolveTime = base::Milliseconds(10);
  absl::optional<HttpssvcMetrics> metrics(querying_experimental_);
  metrics->SaveForAddressQuery(absl::nullopt, kResolveTime,
                               HttpssvcDnsRcode::kNoError);
  metrics.reset();  // Record the metrics to UMA.

  VerifyAddressResolveTimeMetric();
  VerifyIntegrityMetricsForExpectIntact();
  VerifyHttpsMetricsForExpectIntact();
  VerifyIntegrityMetricsForExpectNoerror();
  VerifyHttpsMetricsForExpectNoerror();
}

TEST_P(HttpssvcMetricsTest, AddressAndIntegrityIntact) {
  const base::TimeDelta kResolveTime = base::Milliseconds(10);
  const base::TimeDelta kResolveTimeIntegrity = base::Milliseconds(15);
  absl::optional<HttpssvcMetrics> metrics(querying_experimental_);
  metrics->SaveForIntegrity(absl::nullopt, HttpssvcDnsRcode::kNoError, {true},
                            kResolveTimeIntegrity);
  metrics->SaveForAddressQuery(absl::nullopt, kResolveTime,
                               HttpssvcDnsRcode::kNoError);
  metrics.reset();  // Record the metrics to UMA.

  VerifyHttpsMetricsForExpectIntact();
  VerifyHttpsMetricsForExpectNoerror();

  if (querying_experimental_) {
    VerifyAddressResolveTimeMetric({kResolveTime} /* expect_intact_time */);
    VerifyIntegrityMetricsForExpectIntact(
        absl::nullopt /* rcode */, {true} /* integrity */,
        absl::nullopt /* record_with_error */,
        {kResolveTimeIntegrity} /* resolve_time_integrity */,
        {15} /* resolve_time_ratio */);

    VerifyIntegrityMetricsForExpectNoerror();
    return;
  }

  VerifyIntegrityMetricsForExpectIntact();

  VerifyAddressResolveTimeMetric(absl::nullopt /* expect_intact_time */,
                                 {kResolveTime} /* expect_noerror_time */);
  VerifyIntegrityMetricsForExpectNoerror(
      {HttpssvcDnsRcode::kNoError} /* rcode */, {1} /* record_received */,
      {kResolveTimeIntegrity} /* resolve_time_integrity */,
      {15} /* resolve_time_ratio */);
}

TEST_P(HttpssvcMetricsTest, AddressAndHttpsParsable) {
  const base::TimeDelta kResolveTime = base::Milliseconds(10);
  const base::TimeDelta kResolveTimeHttps = base::Milliseconds(15);
  absl::optional<HttpssvcMetrics> metrics(querying_experimental_);
  metrics->SaveForHttps(absl::nullopt, HttpssvcDnsRcode::kNoError, {true},
                        kResolveTimeHttps);
  metrics->SaveForAddressQuery(absl::nullopt, kResolveTime,
                               HttpssvcDnsRcode::kNoError);
  metrics.reset();  // Record the metrics to UMA.

  VerifyIntegrityMetricsForExpectIntact();
  VerifyIntegrityMetricsForExpectNoerror();

  if (querying_experimental_) {
    VerifyAddressResolveTimeMetric({kResolveTime} /* expect_intact_time */);
    VerifyHttpsMetricsForExpectIntact(
        absl::nullopt /* rcode */, {true} /* parsable */,
        absl::nullopt /* record_with_error */,
        {kResolveTimeHttps} /* resolve_time_https */,
        {15} /* resolve_time_ratio */);

    VerifyHttpsMetricsForExpectNoerror();
    return;
  }

  VerifyHttpsMetricsForExpectIntact();

  VerifyAddressResolveTimeMetric(absl::nullopt /* expect_intact_time */,
                                 {kResolveTime} /* expect_noerror_time */);
  VerifyHttpsMetricsForExpectNoerror(
      {HttpssvcDnsRcode::kNoError} /* rcode */, {true} /* parsable */,
      absl::nullopt /* record_with_error */,
      {kResolveTimeHttps} /* resolve_time_https */,
      {15} /* resolve_time_ratio */);
}

TEST_P(HttpssvcMetricsTest, AddressAndIntegrityIntactAndHttpsParsable) {
  const base::TimeDelta kResolveTime = base::Milliseconds(10);
  const base::TimeDelta kResolveTimeIntegrity = base::Milliseconds(15);
  const base::TimeDelta kResolveTimeHttps = base::Milliseconds(20);
  absl::optional<HttpssvcMetrics> metrics(querying_experimental_);
  metrics->SaveForIntegrity(absl::nullopt, HttpssvcDnsRcode::kNoError, {true},
                            kResolveTimeIntegrity);
  metrics->SaveForHttps(absl::nullopt, HttpssvcDnsRcode::kNoError, {true},
                        kResolveTimeHttps);
  metrics->SaveForAddressQuery(absl::nullopt, kResolveTime,
                               HttpssvcDnsRcode::kNoError);
  metrics.reset();  // Record the metrics to UMA.

  if (querying_experimental_) {
    VerifyAddressResolveTimeMetric({kResolveTime} /* expect_intact_time */);
    VerifyIntegrityMetricsForExpectIntact(
        absl::nullopt /* rcode */, {true} /* integrity */,
        absl::nullopt /* record_with_error */,
        {kResolveTimeIntegrity} /* resolve_time_integrity */,
        {15} /* resolve_time_ratio */);
    VerifyHttpsMetricsForExpectIntact(
        absl::nullopt /* rcode */, {true} /* parsable */,
        absl::nullopt /* record_with_error */,
        {kResolveTimeHttps} /* resolve_time_https */,
        {20} /* resolve_time_ratio */);

    VerifyIntegrityMetricsForExpectNoerror();
    VerifyHttpsMetricsForExpectNoerror();
    return;
  }

  VerifyIntegrityMetricsForExpectIntact();
  VerifyHttpsMetricsForExpectIntact();

  VerifyAddressResolveTimeMetric(absl::nullopt /* expect_intact_time */,
                                 {kResolveTime} /* expect_noerror_time */);
  VerifyIntegrityMetricsForExpectNoerror(
      {HttpssvcDnsRcode::kNoError} /* rcode */, {1} /* record_received */,
      {kResolveTimeIntegrity} /* resolve_time_integrity */,
      {15} /* resolve_time_ratio */);
  VerifyHttpsMetricsForExpectNoerror(
      {HttpssvcDnsRcode::kNoError} /* rcode */, {true} /* parsable */,
      absl::nullopt /* record_with_error */,
      {kResolveTimeHttps} /* resolve_time_https */,
      {20} /* resolve_time_ratio */);
}

// This test simulates an INTEGRITY response that includes no INTEGRITY records,
// but does have an error value for the RCODE.
TEST_P(HttpssvcMetricsTest, AddressAndIntegrityMissingWithRcode) {
  const base::TimeDelta kResolveTime = base::Milliseconds(10);
  const base::TimeDelta kResolveTimeIntegrity = base::Milliseconds(15);

  absl::optional<HttpssvcMetrics> metrics(querying_experimental_);
  metrics->SaveForIntegrity(absl::nullopt, HttpssvcDnsRcode::kNxDomain, {},
                            kResolveTimeIntegrity);
  metrics->SaveForAddressQuery(absl::nullopt, kResolveTime,
                               HttpssvcDnsRcode::kNoError);
  metrics.reset();  // Record the metrics to UMA.

  VerifyHttpsMetricsForExpectIntact();
  VerifyHttpsMetricsForExpectNoerror();

  if (querying_experimental_) {
    VerifyAddressResolveTimeMetric({kResolveTime} /* expect_intact_time */);
    VerifyIntegrityMetricsForExpectIntact(
        {HttpssvcDnsRcode::kNxDomain} /* rcode */,
        absl::nullopt /* integrity */, absl::nullopt /* record_with_error */,
        {kResolveTimeIntegrity} /* resolve_time_integrity */,
        {15} /* resolve_time_ratio */);

    VerifyIntegrityMetricsForExpectNoerror();
    return;
  }

  VerifyIntegrityMetricsForExpectIntact();

  VerifyAddressResolveTimeMetric(absl::nullopt /* expect_intact_time */,
                                 {kResolveTime} /* expect_noerror_time */);
  VerifyIntegrityMetricsForExpectNoerror(
      {HttpssvcDnsRcode::kNxDomain} /* rcode */,
      absl::nullopt /* record_received */,
      {kResolveTimeIntegrity} /* resolve_time_integrity */,
      {15} /* resolve_time_ratio */);
}

// This test simulates an HTTPS response that includes no HTTPS records,
// but does have an error value for the RCODE.
TEST_P(HttpssvcMetricsTest, AddressAndHttpsMissingWithRcode) {
  const base::TimeDelta kResolveTime = base::Milliseconds(10);
  const base::TimeDelta kResolveTimeHttps = base::Milliseconds(15);

  absl::optional<HttpssvcMetrics> metrics(querying_experimental_);
  metrics->SaveForHttps(absl::nullopt, HttpssvcDnsRcode::kNxDomain, {},
                        kResolveTimeHttps);
  metrics->SaveForAddressQuery(absl::nullopt, kResolveTime,
                               HttpssvcDnsRcode::kNoError);
  metrics.reset();  // Record the metrics to UMA.

  VerifyIntegrityMetricsForExpectIntact();
  VerifyIntegrityMetricsForExpectNoerror();

  if (querying_experimental_) {
    VerifyAddressResolveTimeMetric({kResolveTime} /* expect_intact_time */);
    VerifyHttpsMetricsForExpectIntact(
        {HttpssvcDnsRcode::kNxDomain} /* rcode */, absl::nullopt /* parsable */,
        absl::nullopt /* record_with_error */,
        {kResolveTimeHttps} /* resolve_time_https */,
        {15} /* resolve_time_ratio */);

    VerifyHttpsMetricsForExpectNoerror();
    return;
  }

  VerifyHttpsMetricsForExpectIntact();

  VerifyAddressResolveTimeMetric(absl::nullopt /* expect_intact_time */,
                                 {kResolveTime} /* expect_noerror_time */);
  VerifyHttpsMetricsForExpectNoerror(
      {HttpssvcDnsRcode::kNxDomain} /* rcode */, absl::nullopt /* parsable */,
      absl::nullopt /* record_with_error */,
      {kResolveTimeHttps} /* resolve_time_https */,
      {15} /* resolve_time_ratio */);
}

// This test simulates an INTEGRITY response that includes an intact INTEGRITY
// record, but also has an error RCODE.
TEST_P(HttpssvcMetricsTest, AddressAndIntegrityIntactWithRcode) {
  const base::TimeDelta kResolveTime = base::Milliseconds(10);
  const base::TimeDelta kResolveTimeIntegrity = base::Milliseconds(15);

  absl::optional<HttpssvcMetrics> metrics(querying_experimental_);
  metrics->SaveForIntegrity(absl::nullopt, HttpssvcDnsRcode::kNxDomain, {true},
                            kResolveTimeIntegrity);
  metrics->SaveForAddressQuery(absl::nullopt, kResolveTime,
                               HttpssvcDnsRcode::kNoError);
  metrics.reset();  // Record the metrics to UMA.

  VerifyHttpsMetricsForExpectIntact();
  VerifyHttpsMetricsForExpectNoerror();

  if (querying_experimental_) {
    VerifyAddressResolveTimeMetric({kResolveTime} /* expect_intact_time */);
    VerifyIntegrityMetricsForExpectIntact(
        // "DnsRcode" metric is omitted because we received an INTEGRITY record.
        absl::nullopt /* rcode */,
        // "Integrity" metric is omitted because the RCODE is not NOERROR.
        absl::nullopt /* integrity */, {true} /* record_with_error */,
        {kResolveTimeIntegrity} /* resolve_time_integrity */,
        {15} /* resolve_time_ratio */);

    VerifyIntegrityMetricsForExpectNoerror();
    return;
  }

  VerifyIntegrityMetricsForExpectIntact();

  VerifyAddressResolveTimeMetric(absl::nullopt /* expect_intact_time */,
                                 {kResolveTime} /* expect_noerror_time */);
  VerifyIntegrityMetricsForExpectNoerror(
      {HttpssvcDnsRcode::kNxDomain} /* rcode */, {true} /* record_received */,
      {kResolveTimeIntegrity} /* resolve_time_integrity */,
      {15} /* resolve_time_ratio */);
}

// This test simulates an HTTPS response that includes a parsable HTTPS
// record, but also has an error RCODE.
TEST_P(HttpssvcMetricsTest, AddressAndHttpsParsableWithRcode) {
  const base::TimeDelta kResolveTime = base::Milliseconds(10);
  const base::TimeDelta kResolveTimeHttps = base::Milliseconds(15);

  absl::optional<HttpssvcMetrics> metrics(querying_experimental_);
  metrics->SaveForHttps(absl::nullopt, HttpssvcDnsRcode::kNxDomain, {true},
                        kResolveTimeHttps);
  metrics->SaveForAddressQuery(absl::nullopt, kResolveTime,
                               HttpssvcDnsRcode::kNoError);
  metrics.reset();  // Record the metrics to UMA.

  VerifyIntegrityMetricsForExpectIntact();
  VerifyIntegrityMetricsForExpectNoerror();

  if (querying_experimental_) {
    VerifyAddressResolveTimeMetric({kResolveTime} /* expect_intact_time */);
    VerifyHttpsMetricsForExpectIntact(
        // "DnsRcode" metric is omitted because we received an HTTPS record.
        absl::nullopt /* rcode */,
        // "parsable" metric is omitted because the RCODE is not NOERROR.
        absl::nullopt /* parsable */, {true} /* record_with_error */,
        {kResolveTimeHttps} /* resolve_time_https */,
        {15} /* resolve_time_ratio */);

    VerifyHttpsMetricsForExpectNoerror();
    return;
  }

  VerifyHttpsMetricsForExpectIntact();

  VerifyAddressResolveTimeMetric(absl::nullopt /* expect_intact_time */,
                                 {kResolveTime} /* expect_noerror_time */);
  VerifyHttpsMetricsForExpectNoerror(
      {HttpssvcDnsRcode::kNxDomain} /* rcode */,
      // "parsable" metric is omitted because the RCODE is not NOERROR.
      absl::nullopt /* parsable */, {true} /* record_with_error */,
      {kResolveTimeHttps} /* resolve_time_https */,
      {15} /* resolve_time_ratio */);
}

// This test simulates an INTEGRITY response that includes a mangled INTEGRITY
// record *and* has an error RCODE.
TEST_P(HttpssvcMetricsTest, AddressAndIntegrityMangledWithRcode) {
  const base::TimeDelta kResolveTime = base::Milliseconds(10);
  const base::TimeDelta kResolveTimeIntegrity = base::Milliseconds(15);
  absl::optional<HttpssvcMetrics> metrics(querying_experimental_);
  metrics->SaveForIntegrity(absl::nullopt, HttpssvcDnsRcode::kNxDomain, {false},
                            kResolveTimeIntegrity);
  metrics->SaveForAddressQuery(absl::nullopt, kResolveTime,
                               HttpssvcDnsRcode::kNoError);
  metrics.reset();  // Record the metrics to UMA.

  VerifyHttpsMetricsForExpectIntact();
  VerifyHttpsMetricsForExpectNoerror();

  if (querying_experimental_) {
    VerifyAddressResolveTimeMetric({kResolveTime} /* expect_intact_time */);
    VerifyIntegrityMetricsForExpectIntact(
        // "DnsRcode" metric is omitted because we received an INTEGRITY record.
        absl::nullopt /* rcode */,
        // "Integrity" metric is omitted because the RCODE is not NOERROR.
        absl::nullopt /* integrity */, {true} /* record_with_error */,
        {kResolveTimeIntegrity} /* resolve_time_integrity */,
        {15} /* resolve_time_ratio */);

    VerifyIntegrityMetricsForExpectNoerror();
    return;
  }

  VerifyIntegrityMetricsForExpectIntact();

  VerifyAddressResolveTimeMetric(absl::nullopt /* expect_intact_time */,
                                 {kResolveTime} /* expect_noerror_time */);
  VerifyIntegrityMetricsForExpectNoerror(
      {HttpssvcDnsRcode::kNxDomain} /* rcode */, {true} /* record_received */,
      {kResolveTimeIntegrity} /* resolve_time_integrity */,
      {15} /* resolve_time_ratio */);
}

// This test simulates an HTTPS response that includes a mangled HTTPS
// record *and* has an error RCODE.
TEST_P(HttpssvcMetricsTest, AddressAndHttpsMangledWithRcode) {
  const base::TimeDelta kResolveTime = base::Milliseconds(10);
  const base::TimeDelta kResolveTimeHttps = base::Milliseconds(15);
  absl::optional<HttpssvcMetrics> metrics(querying_experimental_);
  metrics->SaveForHttps(absl::nullopt, HttpssvcDnsRcode::kNxDomain, {false},
                        kResolveTimeHttps);
  metrics->SaveForAddressQuery(absl::nullopt, kResolveTime,
                               HttpssvcDnsRcode::kNoError);
  metrics.reset();  // Record the metrics to UMA.

  VerifyIntegrityMetricsForExpectIntact();
  VerifyIntegrityMetricsForExpectNoerror();

  if (querying_experimental_) {
    VerifyAddressResolveTimeMetric({kResolveTime} /* expect_intact_time */);
    VerifyHttpsMetricsForExpectIntact(
        // "DnsRcode" metric is omitted because we received an HTTPS record.
        absl::nullopt /* rcode */,
        // "parsable" metric is omitted because the RCODE is not NOERROR.
        absl::nullopt /* parsable */, {true} /* record_with_error */,
        {kResolveTimeHttps} /* resolve_time_https */,
        {15} /* resolve_time_ratio */);

    VerifyHttpsMetricsForExpectNoerror();
    return;
  }

  VerifyHttpsMetricsForExpectIntact();

  VerifyAddressResolveTimeMetric(absl::nullopt /* expect_intact_time */,
                                 {kResolveTime} /* expect_noerror_time */);
  VerifyHttpsMetricsForExpectNoerror(
      {HttpssvcDnsRcode::kNxDomain} /* rcode */,
      // "parsable" metric is omitted because the RCODE is not NOERROR.
      absl::nullopt /* parsable */, {true} /* record_with_error */,
      {kResolveTimeHttps} /* resolve_time_https */,
      {15} /* resolve_time_ratio */);
}

// This test simulates successful address queries and an INTEGRITY query that
// timed out.
TEST_P(HttpssvcMetricsTest, AddressAndIntegrityTimedOut) {
  const base::TimeDelta kResolveTime = base::Milliseconds(10);
  const base::TimeDelta kResolveTimeIntegrity = base::Milliseconds(15);
  absl::optional<HttpssvcMetrics> metrics(querying_experimental_);
  metrics->SaveForIntegrity(absl::nullopt, HttpssvcDnsRcode::kTimedOut, {},
                            kResolveTimeIntegrity);
  metrics->SaveForAddressQuery(absl::nullopt, kResolveTime,
                               HttpssvcDnsRcode::kNoError);
  metrics.reset();  // Record the metrics to UMA.

  VerifyHttpsMetricsForExpectIntact();
  VerifyHttpsMetricsForExpectNoerror();

  if (querying_experimental_) {
    VerifyAddressResolveTimeMetric({kResolveTime} /* expect_intact_time */);
    VerifyIntegrityMetricsForExpectIntact(
        {HttpssvcDnsRcode::kTimedOut} /* rcode */,
        // "Integrity" metric is omitted because the RCODE is not NOERROR.
        absl::nullopt /* integrity */, absl::nullopt /* record_with_error */,
        {kResolveTimeIntegrity} /* resolve_time_integrity */,
        {15} /* resolve_time_ratio */);

    VerifyIntegrityMetricsForExpectNoerror();
    return;
  }

  VerifyIntegrityMetricsForExpectIntact();

  VerifyAddressResolveTimeMetric(absl::nullopt /* expect_intact_time */,
                                 {kResolveTime} /* expect_noerror_time */);
  VerifyIntegrityMetricsForExpectNoerror(
      {HttpssvcDnsRcode::kTimedOut} /* rcode */,
      absl::nullopt /* record_received */,
      {kResolveTimeIntegrity} /* resolve_time_integrity */,
      {15} /* resolve_time_ratio */);
}

// This test simulates successful address queries and an HTTPS query that
// timed out.
TEST_P(HttpssvcMetricsTest, AddressAndHttpsTimedOut) {
  const base::TimeDelta kResolveTime = base::Milliseconds(10);
  const base::TimeDelta kResolveTimeHttps = base::Milliseconds(15);
  absl::optional<HttpssvcMetrics> metrics(querying_experimental_);
  metrics->SaveForHttps(absl::nullopt, HttpssvcDnsRcode::kTimedOut, {},
                        kResolveTimeHttps);
  metrics->SaveForAddressQuery(absl::nullopt, kResolveTime,
                               HttpssvcDnsRcode::kNoError);
  metrics.reset();  // Record the metrics to UMA.

  VerifyIntegrityMetricsForExpectIntact();
  VerifyIntegrityMetricsForExpectNoerror();

  if (querying_experimental_) {
    VerifyAddressResolveTimeMetric({kResolveTime} /* expect_intact_time */);
    VerifyHttpsMetricsForExpectIntact(
        {HttpssvcDnsRcode::kTimedOut} /* rcode */,
        // "parsable" metric is omitted because the RCODE is not NOERROR.
        absl::nullopt /* parsable */, absl::nullopt /* record_with_error */,
        {kResolveTimeHttps} /* resolve_time_https */,
        {15} /* resolve_time_ratio */);

    VerifyIntegrityMetricsForExpectNoerror();
    return;
  }

  VerifyHttpsMetricsForExpectIntact();

  VerifyAddressResolveTimeMetric(absl::nullopt /* expect_intact_time */,
                                 {kResolveTime} /* expect_noerror_time */);
  VerifyHttpsMetricsForExpectNoerror(
      {HttpssvcDnsRcode::kTimedOut} /* rcode */,
      // "parsable" metric is omitted because the RCODE is not NOERROR.
      absl::nullopt /* parsable */, absl::nullopt /* record_with_error */,
      {kResolveTimeHttps} /* resolve_time_https */,
      {15} /* resolve_time_ratio */);
}

}  // namespace net
