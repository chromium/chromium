// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/cross_origin_resource_policy.h"
#include "services/network/public/mojom/blocked_by_response_reason.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {
class TestCoepReporter final : public mojom::CrossOriginEmbedderPolicyReporter {
 public:
  struct Report {
    Report(const GURL& blocked_url,
           mojom::RequestDestination destination,
           bool report_only)
        : blocked_url(blocked_url),
          destination(destination),
          report_only(report_only) {}

    const GURL blocked_url;
    const mojom::RequestDestination destination;
    const bool report_only;
  };

  TestCoepReporter() = default;
  ~TestCoepReporter() override = default;
  TestCoepReporter(const TestCoepReporter&) = delete;
  TestCoepReporter& operator=(const TestCoepReporter&) = delete;

  // mojom::CrossOriginEmbedderPolicyReporter implementation.
  void QueueCorpViolationReport(const GURL& blocked_url,
                                mojom::RequestDestination destination,
                                bool report_only) override {
    reports_.emplace_back(blocked_url, destination, report_only);
  }
  void Clone(
      mojo::PendingReceiver<network::mojom::CrossOriginEmbedderPolicyReporter>
          receiver) override {
    NOTREACHED();
  }

  const std::vector<Report>& reports() const { return reports_; }
  void ClearReports() { reports_.clear(); }

 private:
  std::vector<Report> reports_;
};

class TestDipReporter final : public mojom::DocumentIsolationPolicyReporter {
 public:
  struct Report {
    Report(const GURL& blocked_url,
           mojom::RequestDestination destination,
           bool report_only)
        : blocked_url(blocked_url),
          destination(destination),
          report_only(report_only) {}

    const GURL blocked_url;
    const mojom::RequestDestination destination;
    const bool report_only;
  };

  TestDipReporter() = default;
  ~TestDipReporter() override = default;
  TestDipReporter(const TestDipReporter&) = delete;
  TestDipReporter& operator=(const TestDipReporter&) = delete;

  // mojom::CrossOriginEmbedderPolicyReporter implementation.
  void QueueCorpViolationReport(const GURL& blocked_url,
                                mojom::RequestDestination destination,
                                bool report_only) override {
    reports_.emplace_back(blocked_url, destination, report_only);
  }
  void Clone(
      mojo::PendingReceiver<network::mojom::DocumentIsolationPolicyReporter>
          receiver) override {
    NOTREACHED();
  }

  const std::vector<Report>& reports() const { return reports_; }
  void ClearReports() { reports_.clear(); }

 private:
  std::vector<Report> reports_;
};

}  // namespace

CrossOriginResourcePolicy::ParsedHeader ParseHeader(
    const std::string& test_headers) {
  std::string all_headers = "HTTP/1.1 200 OK\n" + test_headers + "\n";
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(all_headers));
  return CrossOriginResourcePolicy::ParseHeaderForTesting(headers.get());
}

// This test is somewhat redundant with
// wpt/fetch/cross-origin-resource-policy/syntax.any.js
// The delta in coverage is mostly around testing case insensitivity of the
// header name.
TEST(CrossOriginResourcePolicyTest, ParseHeader) {
  // Basic tests.
  EXPECT_EQ(CrossOriginResourcePolicy::kNoHeader, ParseHeader(""));
  EXPECT_EQ(CrossOriginResourcePolicy::kSameOrigin,
            ParseHeader("Cross-Origin-Resource-Policy: same-origin"));
  EXPECT_EQ(CrossOriginResourcePolicy::kSameSite,
            ParseHeader("Cross-Origin-Resource-Policy: same-site"));

  // Header names are case-insensitive.
  EXPECT_EQ(CrossOriginResourcePolicy::kSameOrigin,
            ParseHeader("Cross-Origin-RESOURCE-Policy: same-origin"));
  EXPECT_EQ(CrossOriginResourcePolicy::kSameSite,
            ParseHeader("Cross-ORIGIN-Resource-Policy: same-site"));

  // Header values are case-sensitive.
  EXPECT_EQ(CrossOriginResourcePolicy::kParsingError,
            ParseHeader("Cross-Origin-Resource-Policy: sAme-origin"));
  EXPECT_EQ(CrossOriginResourcePolicy::kParsingError,
            ParseHeader("Cross-Origin-Resource-Policy: saMe-site"));

  // Specific origins are not yet part of the spec.  See also:
  // https://github.com/whatwg/fetch/issues/760
  EXPECT_EQ(
      CrossOriginResourcePolicy::kParsingError,
      ParseHeader("Cross-Origin-Resource-Policy: https://www.example.com"));

  // Parsing failures explicitly called out in the note for step 3:
  // https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header:
  //
  //   > This means that `Cross-Origin-Resource-Policy: same-site, same-origin`
  //   > ends up as allowed below as it will never match anything. Two or more
  //   > `Cross-Origin-Resource-Policy` headers will have the same effect.
  //
  EXPECT_EQ(
      CrossOriginResourcePolicy::kParsingError,
      ParseHeader("Cross-Origin-Resource-Policy: same-site, same-origin"));
  EXPECT_EQ(CrossOriginResourcePolicy::kParsingError,
            ParseHeader("Cross-Origin-Resource-Policy: same-site\n"
                        "Cross-Origin-Resource-Policy: same-origin"));
}

TEST(CrossOriginResourcePolicyTest, CrossSiteHeaderWithCOEP) {
  EXPECT_EQ(CrossOriginResourcePolicy::kCrossOrigin,
            ParseHeader("Cross-Origin-Resource-Policy: cross-origin"));
}

bool ShouldAllowSameSite(const std::string& initiator,
                         const std::string& target) {
  return CrossOriginResourcePolicy::ShouldAllowSameSiteForTesting(
      url::Origin::Create(GURL(initiator)), url::Origin::Create(GURL(target)));
}

TEST(CrossOriginResourcePolicyTest, ShouldAllowSameSite) {
  // Basic tests.
  EXPECT_TRUE(ShouldAllowSameSite("https://foo.com", "https://foo.com"));
  EXPECT_FALSE(ShouldAllowSameSite("https://foo.com", "https://bar.com"));

  // Subdomains.
  EXPECT_TRUE(ShouldAllowSameSite("https://foo.a.com", "https://a.com"));
  EXPECT_TRUE(ShouldAllowSameSite("https://a.com", "https://bar.a.com"));
  EXPECT_TRUE(ShouldAllowSameSite("https://foo.a.com", "https://bar.a.com"));
  EXPECT_FALSE(ShouldAllowSameSite("https://foo.a.com", "https://b.com"));
  EXPECT_FALSE(ShouldAllowSameSite("https://a.com", "https://bar.b.com"));
  EXPECT_FALSE(ShouldAllowSameSite("https://foo.a.com", "https://bar.b.com"));

  // Same host, different HTTPS vs HTTP scheme.
  //
  // The intent here is that HTTPS response shouldn't be exposed to a page
  // served over HTTP (which might leak it), but serving an HTTP response isn't
  // secret so we don't need to block it from HTTPS pages.  This behavior should
  // hopefully help with adoption on sites that still need to use mixed
  // http/https content.
  EXPECT_TRUE(ShouldAllowSameSite(
      /* initiator = */ "https://foo.com", /* target = */ "http://foo.com"));
  EXPECT_FALSE(ShouldAllowSameSite(
      /* initiator = */ "http://foo.com", /* target = */ "https://foo.com"));

  // IP addresses.
  //
  // Different sites might be served from the same IP address - they should
  // still be considered to be different sites - see also
  // https://url.spec.whatwg.org/#host-same-site which excludes IP addresses by
  // imposing the requirement that one of the addresses has to have a non-null
  // registrable domain.
  EXPECT_FALSE(ShouldAllowSameSite("http://127.0.0.1", "http://127.0.0.1"));
}

// Tracks the reporting expectations for tests with COEP and
// DocumentIsolationPolicy. By default, no report should be sent.
struct ReportingExpectations {
  bool expect_coep_report = false;
  bool expect_coep_report_only = false;
  bool expect_dip_report = false;
  bool expect_dip_report_only = false;
};

void CheckCORP(mojom::RequestMode request_mode,
               const url::Origin& origin,
               mojom::URLResponseHeadPtr response_info,
               const CrossOriginEmbedderPolicy& coep,
               const DocumentIsolationPolicy& dip,
               std::optional<mojom::BlockedByResponseReason> expected_result,
               const ReportingExpectations& reporting) {
  using mojom::RequestDestination;
  const GURL original_url("https://original.example.com/x/y");
  const GURL final_url("https://www.example.com/z/u");
  TestCoepReporter coep_reporter;
  TestDipReporter dip_reporter;

  // Check that the result matches the expectations.
  EXPECT_EQ(expected_result,
            CrossOriginResourcePolicy::IsBlocked(
                final_url, original_url, origin, *response_info, request_mode,
                RequestDestination::kImage, coep, &coep_reporter, dip,
                &dip_reporter));

  // Check that the right COEP reports were emitted.
  if (reporting.expect_coep_report && reporting.expect_coep_report_only) {
    ASSERT_EQ(2u, coep_reporter.reports().size());
    EXPECT_TRUE(coep_reporter.reports()[0].report_only);
    EXPECT_EQ(coep_reporter.reports()[0].blocked_url, original_url);
    EXPECT_EQ(coep_reporter.reports()[0].destination,
              RequestDestination::kImage);
    EXPECT_FALSE(coep_reporter.reports()[1].report_only);
    EXPECT_EQ(coep_reporter.reports()[1].blocked_url, original_url);
    EXPECT_EQ(coep_reporter.reports()[1].destination,
              RequestDestination::kImage);
  } else if (reporting.expect_coep_report) {
    ASSERT_EQ(1u, coep_reporter.reports().size());
    EXPECT_FALSE(coep_reporter.reports()[0].report_only);
    EXPECT_EQ(coep_reporter.reports()[0].blocked_url, original_url);
    EXPECT_EQ(coep_reporter.reports()[0].destination,
              RequestDestination::kImage);
  } else if (reporting.expect_coep_report_only) {
    ASSERT_EQ(1u, coep_reporter.reports().size());
    EXPECT_TRUE(coep_reporter.reports()[0].report_only);
    EXPECT_EQ(coep_reporter.reports()[0].blocked_url, original_url);
    EXPECT_EQ(coep_reporter.reports()[0].destination,
              RequestDestination::kImage);
  } else {
    EXPECT_TRUE(coep_reporter.reports().empty());
  }

  // Check that the right DocumentIsolationPolicy reports were emitted.
  if (reporting.expect_dip_report && reporting.expect_dip_report_only) {
    ASSERT_EQ(2u, dip_reporter.reports().size());
    EXPECT_TRUE(dip_reporter.reports()[0].report_only);
    EXPECT_EQ(dip_reporter.reports()[0].blocked_url, original_url);
    EXPECT_EQ(dip_reporter.reports()[0].destination,
              RequestDestination::kImage);
    EXPECT_FALSE(dip_reporter.reports()[1].report_only);
    EXPECT_EQ(dip_reporter.reports()[1].blocked_url, original_url);
    EXPECT_EQ(dip_reporter.reports()[1].destination,
              RequestDestination::kImage);
  } else if (reporting.expect_dip_report) {
    ASSERT_EQ(1u, dip_reporter.reports().size());
    EXPECT_FALSE(dip_reporter.reports()[0].report_only);
    EXPECT_EQ(dip_reporter.reports()[0].blocked_url, original_url);
    EXPECT_EQ(dip_reporter.reports()[0].destination,
              RequestDestination::kImage);
  } else if (reporting.expect_dip_report_only) {
    ASSERT_EQ(1u, dip_reporter.reports().size());
    EXPECT_TRUE(dip_reporter.reports()[0].report_only);
    EXPECT_EQ(dip_reporter.reports()[0].blocked_url, original_url);
    EXPECT_EQ(dip_reporter.reports()[0].destination,
              RequestDestination::kImage);
  } else {
    EXPECT_TRUE(dip_reporter.reports().empty());
  }
}

void RunCORPTestWithCOEPAndDIP(
    const CrossOriginEmbedderPolicy& coep,
    const DocumentIsolationPolicy& dip,
    std::optional<mojom::BlockedByResponseReason> expected_result,
    bool include_credentials,
    const ReportingExpectations& reporting_expectations) {
  mojom::URLResponseHead corp_none;
  mojom::URLResponseHead corp_same_origin;
  mojom::URLResponseHead corp_cross_origin;

  corp_same_origin.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(
          "HTTP/1.1 200 OK\n"
          "cross-origin-resource-policy: same-origin\n"));

  corp_cross_origin.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(
          "HTTP/1.1 200 OK\n"
          "cross-origin-resource-policy: cross-origin\n"));

  corp_none.request_include_credentials = include_credentials;
  corp_same_origin.request_include_credentials = include_credentials;
  corp_cross_origin.request_include_credentials = include_credentials;

  constexpr auto kAllow = std::nullopt;
  using mojom::RequestMode;

  url::Origin destination_origin =
      url::Origin::Create(GURL("https://www.example.com"));
  url::Origin another_origin =
      url::Origin::Create(GURL("https://www2.example.com"));

  // First check that COEP and DIP do not affect the following test cases.

  // 1. Responses with "cross-origin-resource-policy: same-origin" are always
  // blocked when requested cross-origin. No report should be sent.
  CheckCORP(RequestMode::kNoCors, another_origin, corp_same_origin.Clone(),
            coep, dip, mojom::BlockedByResponseReason::kCorpNotSameOrigin,
            ReportingExpectations());

  // 2. Responses with "cross-origin-resource-policy: cross-origin" are always
  // allowed. No report should be sent.
  CheckCORP(RequestMode::kNoCors, another_origin, corp_cross_origin.Clone(),
            coep, dip, kAllow, ReportingExpectations());

  // 3. Same-origin responses are always allowed. No report should be sent.
  CheckCORP(RequestMode::kNoCors, destination_origin, corp_same_origin.Clone(),
            coep, dip, kAllow, ReportingExpectations());

  // 4. Requests whose mode is "cors" are always allowed. No report should be
  // sent.
  CheckCORP(RequestMode::kCors, another_origin, corp_same_origin.Clone(), coep,
            dip, kAllow, ReportingExpectations());

  // Now check that a cross-origin request without a CORP header behaves as
  // expected. Report sent should match the expectations passed to the test.
  CheckCORP(RequestMode::kNoCors, another_origin, corp_none.Clone(), coep, dip,
            expected_result, reporting_expectations);
}

TEST(CrossOriginResourcePolicyTest, WithCOEPAndDIP) {
  mojom::CrossOriginEmbedderPolicyValue coep_values[] = {
      mojom::CrossOriginEmbedderPolicyValue::kNone,
      mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
      mojom::CrossOriginEmbedderPolicyValue::kCredentialless};
  mojom::DocumentIsolationPolicyValue dip_values[] = {
      mojom::DocumentIsolationPolicyValue::kNone,
      mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
      mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless};

  for (const auto& coep_value : coep_values) {
    for (const auto& coep_report_only_value : coep_values) {
      for (const auto& dip_value : dip_values) {
        for (const auto& dip_report_only_value : dip_values) {
          CrossOriginEmbedderPolicy coep;
          coep.value = coep_value;
          coep.report_only_value = coep_report_only_value;
          DocumentIsolationPolicy dip;
          dip.value = dip_value;
          dip.report_only_value = dip_report_only_value;

          // Set up the expected result. Non kNone values of COEP and DIP should
          // result in cross-origin requests without CORP headers being blocked.
          std::optional<mojom::BlockedByResponseReason> expected_result;
          if (coep_value != mojom::CrossOriginEmbedderPolicyValue::kNone &&
              dip_value != mojom::DocumentIsolationPolicyValue::kNone) {
            expected_result = mojom::BlockedByResponseReason::
                kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip;
          } else if (coep_value !=
                     mojom::CrossOriginEmbedderPolicyValue::kNone) {
            expected_result = mojom::BlockedByResponseReason::
                kCorpNotSameOriginAfterDefaultedToSameOriginByCoep;
          } else if (dip_value != mojom::DocumentIsolationPolicyValue::kNone) {
            expected_result = mojom::BlockedByResponseReason::
                kCorpNotSameOriginAfterDefaultedToSameOriginByDip;
          }

          // Set up the expected reports. Non kNone vealues of COEP and DIP
          // should result in a report being sent.
          ReportingExpectations reporting;
          reporting.expect_coep_report =
              coep_value != mojom::CrossOriginEmbedderPolicyValue::kNone;
          reporting.expect_coep_report_only =
              coep_report_only_value !=
              mojom::CrossOriginEmbedderPolicyValue::kNone;
          reporting.expect_dip_report =
              dip_value != mojom::DocumentIsolationPolicyValue::kNone;
          reporting.expect_dip_report_only =
              dip_report_only_value !=
              mojom::DocumentIsolationPolicyValue::kNone;

          RunCORPTestWithCOEPAndDIP(coep, dip, expected_result,
                                    true /*include_credentials*/, reporting);
        }
      }
    }
  }
}

TEST(CrossOriginResourcePolicyTest, WithCOEPAndDIPNoCredentials) {
  mojom::CrossOriginEmbedderPolicyValue coep_values[] = {
      mojom::CrossOriginEmbedderPolicyValue::kNone,
      mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
      mojom::CrossOriginEmbedderPolicyValue::kCredentialless};
  mojom::DocumentIsolationPolicyValue dip_values[] = {
      mojom::DocumentIsolationPolicyValue::kNone,
      mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
      mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless};

  for (const auto& coep_value : coep_values) {
    for (const auto& coep_report_only_value : coep_values) {
      for (const auto& dip_value : dip_values) {
        for (const auto& dip_report_only_value : dip_values) {
          CrossOriginEmbedderPolicy coep;
          coep.value = coep_value;
          coep.report_only_value = coep_report_only_value;
          DocumentIsolationPolicy dip;
          dip.value = dip_value;
          dip.report_only_value = dip_report_only_value;

          // Set up the expected result. COEP require-corp and DIP
          // isolate-and-require-corp should result in cross-origin requests
          // without CORP headers being blocked.
          std::optional<mojom::BlockedByResponseReason> expected_result;
          if (coep_value ==
                  mojom::CrossOriginEmbedderPolicyValue::kRequireCorp &&
              dip_value ==
                  mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp) {
            expected_result = mojom::BlockedByResponseReason::
                kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip;
          } else if (coep_value ==
                     mojom::CrossOriginEmbedderPolicyValue::kRequireCorp) {
            expected_result = mojom::BlockedByResponseReason::
                kCorpNotSameOriginAfterDefaultedToSameOriginByCoep;
          } else if (dip_value == mojom::DocumentIsolationPolicyValue::
                                      kIsolateAndRequireCorp) {
            expected_result = mojom::BlockedByResponseReason::
                kCorpNotSameOriginAfterDefaultedToSameOriginByDip;
          }

          // Set up the expected reports. COEP require-corp and DIP
          // isolate-and-require-corp should result in a report being sent.
          ReportingExpectations reporting;
          reporting.expect_coep_report =
              coep_value == mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
          reporting.expect_coep_report_only =
              coep_report_only_value ==
              mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
          reporting.expect_dip_report =
              dip_value ==
              mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp;
          reporting.expect_dip_report_only =
              dip_report_only_value ==
              mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp;

          RunCORPTestWithCOEPAndDIP(coep, dip, expected_result,
                                    false /*include_credentials*/, reporting);
        }
      }
    }
  }
}

TEST(CrossOriginResourcePolicyTest, NavigationWithCOEP) {
  mojom::URLResponseHead corp_none;
  mojom::URLResponseHead corp_same_origin;
  mojom::URLResponseHead corp_cross_origin;

  corp_same_origin.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(
          "HTTP/1.1 200 OK\n"
          "cross-origin-resource-policy: same-origin\n"));

  corp_cross_origin.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(
          "HTTP/1.1 200 OK\n"
          "cross-origin-resource-policy: cross-origin\n"));

  GURL original_url("https://original.example.com/x/y");
  GURL final_url("https://www.example.com/z/u");

  url::Origin destination_origin =
      url::Origin::Create(GURL("https://www.example.com"));
  url::Origin another_origin =
      url::Origin::Create(GURL("https://www2.example.com"));

  constexpr auto kAllow = std::nullopt;
  using mojom::RequestDestination;
  using mojom::RequestMode;

  struct TestCase {
    const url::Origin origin;
    mojom::URLResponseHeadPtr response_info;
    const std::optional<mojom::BlockedByResponseReason>
        expectation_with_coep_none;
    const std::optional<mojom::BlockedByResponseReason>
        expectation_with_coep_require_corp;
    const std::optional<mojom::BlockedByResponseReason>
        expectation_with_coep_credentialless;
  } test_cases[] = {
      // We don't have a cross-origin-resource-policy header on a response. That
      // leads to blocking when COEP: kRequireCorp is used.
      {another_origin, corp_none.Clone(), kAllow,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoep,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoep},
      // We have "cross-origin-resource-policy: same-origin",
      // COEP the response is blocked.
      {another_origin, corp_same_origin.Clone(), kAllow,
       mojom::BlockedByResponseReason::kCorpNotSameOrigin,
       mojom::BlockedByResponseReason::kCorpNotSameOrigin},
      // We have "cross-origin-resource-policy: cross-origin", so regardless of
      // COEP the response is allowed.
      {another_origin, corp_cross_origin.Clone(), kAllow, kAllow, kAllow},
      // The origin of the request URL and request's origin match, so regardless
      // of COEP the response is allowed.
      {destination_origin, corp_same_origin.Clone(), kAllow, kAllow, kAllow},
  };

  for (const auto& test_case : test_cases) {
    TestCoepReporter reporter;
    CrossOriginEmbedderPolicy embedder_policy;
    const bool should_be_blocked_due_to_coep =
        (test_case.expectation_with_coep_none !=
         test_case.expectation_with_coep_require_corp);
    const bool should_be_blocked_due_to_credentialless =
        (test_case.expectation_with_coep_none !=
         test_case.expectation_with_coep_credentialless);

    // COEP: none, COEP-report-only: none
    EXPECT_EQ(
        test_case.expectation_with_coep_none,
        CrossOriginResourcePolicy::IsNavigationBlocked(
            final_url, original_url, test_case.origin, *test_case.response_info,
            RequestDestination::kImage, embedder_policy, &reporter));

    EXPECT_TRUE(reporter.reports().empty());

    reporter.ClearReports();
    // COEP: require-corp, COEP-report-only: none
    embedder_policy.value = mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
    EXPECT_EQ(
        test_case.expectation_with_coep_require_corp,
        CrossOriginResourcePolicy::IsNavigationBlocked(
            final_url, original_url, test_case.origin, *test_case.response_info,
            RequestDestination::kImage, embedder_policy, &reporter));
    if (should_be_blocked_due_to_coep) {
      ASSERT_EQ(1u, reporter.reports().size());
      EXPECT_FALSE(reporter.reports()[0].report_only);
      EXPECT_EQ(reporter.reports()[0].blocked_url, original_url);
      EXPECT_EQ(reporter.reports()[0].destination, RequestDestination::kImage);
    } else {
      EXPECT_TRUE(reporter.reports().empty());
    }

    reporter.ClearReports();
    // COEP: none, COEP-report-only: require-corp
    embedder_policy.value = mojom::CrossOriginEmbedderPolicyValue::kNone;
    embedder_policy.report_only_value =
        mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
    EXPECT_EQ(
        test_case.expectation_with_coep_none,
        CrossOriginResourcePolicy::IsNavigationBlocked(
            final_url, original_url, test_case.origin, *test_case.response_info,
            RequestDestination::kScript, embedder_policy, &reporter));
    if (should_be_blocked_due_to_coep) {
      ASSERT_EQ(1u, reporter.reports().size());
      EXPECT_TRUE(reporter.reports()[0].report_only);
      EXPECT_EQ(reporter.reports()[0].blocked_url, original_url);
      EXPECT_EQ(reporter.reports()[0].destination, RequestDestination::kScript);
    } else {
      EXPECT_TRUE(reporter.reports().empty());
    }

    reporter.ClearReports();
    // COEP: require-corp, COEP-report-only: require-corp
    embedder_policy.value = mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
    embedder_policy.report_only_value =
        mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
    EXPECT_EQ(
        test_case.expectation_with_coep_require_corp,
        CrossOriginResourcePolicy::IsNavigationBlocked(
            final_url, original_url, test_case.origin, *test_case.response_info,
            RequestDestination::kEmpty, embedder_policy, &reporter));
    if (should_be_blocked_due_to_coep) {
      ASSERT_EQ(2u, reporter.reports().size());
      EXPECT_TRUE(reporter.reports()[0].report_only);
      EXPECT_EQ(reporter.reports()[0].blocked_url, original_url);
      EXPECT_EQ(reporter.reports()[0].destination, RequestDestination::kEmpty);
      EXPECT_FALSE(reporter.reports()[1].report_only);
      EXPECT_EQ(reporter.reports()[1].blocked_url, original_url);
      EXPECT_EQ(reporter.reports()[1].destination, RequestDestination::kEmpty);
    } else {
      EXPECT_TRUE(reporter.reports().empty());
    }

    reporter.ClearReports();
    // COEP: credentialless, COEP-report-only: none
    embedder_policy.value =
        mojom::CrossOriginEmbedderPolicyValue::kCredentialless;
    EXPECT_EQ(
        test_case.expectation_with_coep_credentialless,
        CrossOriginResourcePolicy::IsNavigationBlocked(
            final_url, original_url, test_case.origin, *test_case.response_info,
            RequestDestination::kImage, embedder_policy, &reporter));
    if (should_be_blocked_due_to_credentialless) {
      ASSERT_EQ(2u, reporter.reports().size());
      EXPECT_TRUE(reporter.reports()[0].report_only);
      EXPECT_EQ(reporter.reports()[0].blocked_url, original_url);
      EXPECT_EQ(reporter.reports()[0].destination, RequestDestination::kImage);
      EXPECT_FALSE(reporter.reports()[1].report_only);
      EXPECT_EQ(reporter.reports()[1].blocked_url, original_url);
      EXPECT_EQ(reporter.reports()[1].destination, RequestDestination::kImage);
    } else {
      EXPECT_TRUE(reporter.reports().empty());
    }

    reporter.ClearReports();
    // COEP: none, COEP-report-only: credentialless
    embedder_policy.value = mojom::CrossOriginEmbedderPolicyValue::kNone;
    embedder_policy.report_only_value =
        mojom::CrossOriginEmbedderPolicyValue::kCredentialless;
    EXPECT_EQ(
        test_case.expectation_with_coep_none,
        CrossOriginResourcePolicy::IsNavigationBlocked(
            final_url, original_url, test_case.origin, *test_case.response_info,
            RequestDestination::kScript, embedder_policy, &reporter));
    if (should_be_blocked_due_to_credentialless) {
      ASSERT_EQ(1u, reporter.reports().size());
      EXPECT_TRUE(reporter.reports()[0].report_only);
      EXPECT_EQ(reporter.reports()[0].blocked_url, original_url);
      EXPECT_EQ(reporter.reports()[0].destination, RequestDestination::kScript);
    } else {
      EXPECT_TRUE(reporter.reports().empty());
    }

    reporter.ClearReports();
    // COEP: credentialless, COEP-report-only: credentialless
    embedder_policy.value =
        mojom::CrossOriginEmbedderPolicyValue::kCredentialless;
    embedder_policy.report_only_value =
        mojom::CrossOriginEmbedderPolicyValue::kCredentialless;
    EXPECT_EQ(
        test_case.expectation_with_coep_credentialless,
        CrossOriginResourcePolicy::IsNavigationBlocked(
            final_url, original_url, test_case.origin, *test_case.response_info,
            RequestDestination::kEmpty, embedder_policy, &reporter));
    if (should_be_blocked_due_to_credentialless) {
      ASSERT_EQ(2u, reporter.reports().size());
      EXPECT_TRUE(reporter.reports()[0].report_only);
      EXPECT_EQ(reporter.reports()[0].blocked_url, original_url);
      EXPECT_EQ(reporter.reports()[0].destination, RequestDestination::kEmpty);
      EXPECT_FALSE(reporter.reports()[1].report_only);
      EXPECT_EQ(reporter.reports()[1].blocked_url, original_url);
      EXPECT_EQ(reporter.reports()[1].destination, RequestDestination::kEmpty);
    } else {
      EXPECT_TRUE(reporter.reports().empty());
    }
  }
}

}  // namespace network
