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
    NOTREACHED_IN_MIGRATION();
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

void CheckCORP(mojom::RequestMode request_mode,
               const url::Origin& origin,
               mojom::URLResponseHeadPtr response_info,
               const CrossOriginEmbedderPolicy& coep,
               const DocumentIsolationPolicy& dip,
               std::optional<mojom::BlockedByResponseReason> expected_result,
               bool expect_coep_report,
               bool expect_coep_report_only) {
  using mojom::RequestDestination;
  const GURL original_url("https://original.example.com/x/y");
  const GURL final_url("https://www.example.com/z/u");
  TestCoepReporter reporter;
  EXPECT_EQ(expected_result,
            CrossOriginResourcePolicy::IsBlocked(
                final_url, original_url, origin, *response_info, request_mode,
                RequestDestination::kImage, coep, &reporter, dip));
  if (expect_coep_report && expect_coep_report_only) {
    ASSERT_EQ(2u, reporter.reports().size());
    EXPECT_TRUE(reporter.reports()[0].report_only);
    EXPECT_EQ(reporter.reports()[0].blocked_url, original_url);
    EXPECT_EQ(reporter.reports()[0].destination, RequestDestination::kImage);
    EXPECT_FALSE(reporter.reports()[1].report_only);
    EXPECT_EQ(reporter.reports()[1].blocked_url, original_url);
    EXPECT_EQ(reporter.reports()[1].destination, RequestDestination::kImage);
  } else if (expect_coep_report) {
    ASSERT_EQ(1u, reporter.reports().size());
    EXPECT_FALSE(reporter.reports()[0].report_only);
    EXPECT_EQ(reporter.reports()[0].blocked_url, original_url);
    EXPECT_EQ(reporter.reports()[0].destination, RequestDestination::kImage);
  } else if (expect_coep_report_only) {
    ASSERT_EQ(1u, reporter.reports().size());
    EXPECT_TRUE(reporter.reports()[0].report_only);
    EXPECT_EQ(reporter.reports()[0].blocked_url, original_url);
    EXPECT_EQ(reporter.reports()[0].destination, RequestDestination::kImage);
  } else {
    EXPECT_TRUE(reporter.reports().empty());
  }
}

void RunCORPTestWithCOEPAndDIP(
    const CrossOriginEmbedderPolicy& coep,
    const DocumentIsolationPolicy& dip,
    std::optional<mojom::BlockedByResponseReason> expected_result,
    bool include_credentials,
    bool expect_coep_report,
    bool expect_coep_report_only) {
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
  // blocked when requested cross-origin.
  CheckCORP(RequestMode::kNoCors, another_origin, corp_same_origin.Clone(),
            coep, dip, mojom::BlockedByResponseReason::kCorpNotSameOrigin,
            false, false);

  // 2. Responses with "cross-origin-resource-policy: cross-origin" are always
  // allowed.
  CheckCORP(RequestMode::kNoCors, another_origin, corp_cross_origin.Clone(),
            coep, dip, kAllow, false, false);

  // 3. Same-origin responses are always allowed.
  CheckCORP(RequestMode::kNoCors, destination_origin, corp_same_origin.Clone(),
            coep, dip, kAllow, false, false);

  // 4. Requests whose mode is "cors" are always allowed.
  CheckCORP(RequestMode::kCors, another_origin, corp_same_origin.Clone(), coep,
            dip, kAllow, false, false);

  // Now check that a cross-origin request without a CORP header behaves as
  // expected.
  CheckCORP(RequestMode::kNoCors, another_origin, corp_none.Clone(), coep, dip,
            expected_result, expect_coep_report, expect_coep_report_only);
}

TEST(CrossOriginResourcePolicyTest, WithCOEP) {
  constexpr auto kAllow = std::nullopt;

  struct TestCase {
    mojom::CrossOriginEmbedderPolicyValue coep_value;
    mojom::CrossOriginEmbedderPolicyValue coep_report_only_value;
    const std::optional<mojom::BlockedByResponseReason> expected_result;
    bool expect_coep_report;
    bool expect_coep_report_only;
  } test_cases[] = {
      {mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::CrossOriginEmbedderPolicyValue::kNone, kAllow, false, false},
      {mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoep,
       true, false},
      {mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoep,
       true, false},
      {mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::CrossOriginEmbedderPolicyValue::kRequireCorp, kAllow, false,
       true},
      {mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoep,
       true, true},
      {mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoep,
       true, true},
      {mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::CrossOriginEmbedderPolicyValue::kCredentialless, kAllow, false,
       false},
      {mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoep,
       true, false},
      {mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoep,
       true, false},
  };

  for (const auto& test_case : test_cases) {
    CrossOriginEmbedderPolicy coep;
    coep.value = test_case.coep_value;
    coep.report_only_value = test_case.coep_report_only_value;
    DocumentIsolationPolicy dip;
    RunCORPTestWithCOEPAndDIP(
        coep, dip, test_case.expected_result, true /*include_credentials*/,
        test_case.expect_coep_report, test_case.expect_coep_report_only);
  }
}

TEST(CrossOriginResourcePolicyTest, WithCOEPNoCredentials) {
  constexpr auto kAllow = std::nullopt;

  struct TestCase {
    mojom::CrossOriginEmbedderPolicyValue coep_value;
    mojom::CrossOriginEmbedderPolicyValue coep_report_only_value;
    const std::optional<mojom::BlockedByResponseReason> expected_result;
    bool expect_coep_report;
    bool expect_coep_report_only;
  } test_cases[] = {
      {mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::CrossOriginEmbedderPolicyValue::kNone, kAllow, false, false},
      {mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoep,
       true, false},
      {mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::CrossOriginEmbedderPolicyValue::kNone, kAllow, false, false},
      {mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::CrossOriginEmbedderPolicyValue::kRequireCorp, kAllow, false,
       true},
      {mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoep,
       true, true},
      {mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::CrossOriginEmbedderPolicyValue::kRequireCorp, kAllow, false,
       true},
      {mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::CrossOriginEmbedderPolicyValue::kCredentialless, kAllow, false,
       false},
      {mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoep,
       true, false},
      {mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::CrossOriginEmbedderPolicyValue::kCredentialless, kAllow, false,
       false},
  };

  for (const auto& test_case : test_cases) {
    CrossOriginEmbedderPolicy coep;
    coep.value = test_case.coep_value;
    coep.report_only_value = test_case.coep_report_only_value;
    DocumentIsolationPolicy dip;
    RunCORPTestWithCOEPAndDIP(
        coep, dip, test_case.expected_result, false /*include_credentials*/,
        test_case.expect_coep_report, test_case.expect_coep_report_only);
  }
}

TEST(CrossOriginResourcePolicyTest, WithDIP) {
  constexpr auto kAllow = std::nullopt;

  struct TestCase {
    mojom::DocumentIsolationPolicyValue dip_value;
    const std::optional<mojom::BlockedByResponseReason> expected_result;
  } test_cases[] = {
      {mojom::DocumentIsolationPolicyValue::kNone, kAllow},
      {mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByDip},
      {mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByDip},
  };

  for (const auto& test_case : test_cases) {
    CrossOriginEmbedderPolicy coep;
    DocumentIsolationPolicy dip(test_case.dip_value);
    RunCORPTestWithCOEPAndDIP(
        coep, dip, test_case.expected_result, true /*include_credentials*/,
        false /*expect_coep_report*/, false /*expect_coep_report_only*/);
  }
}

TEST(CrossOriginResourcePolicyTest, WithDIPNoCredentials) {
  constexpr auto kAllow = std::nullopt;

  struct TestCase {
    mojom::DocumentIsolationPolicyValue dip_value;
    const std::optional<mojom::BlockedByResponseReason> expected_result;
  } test_cases[] = {
      {mojom::DocumentIsolationPolicyValue::kNone, kAllow},
      {mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByDip},
      {mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless, kAllow},
  };

  for (const auto& test_case : test_cases) {
    CrossOriginEmbedderPolicy coep;
    DocumentIsolationPolicy dip(test_case.dip_value);
    RunCORPTestWithCOEPAndDIP(
        coep, dip, test_case.expected_result, false /*include_credentials*/,
        false /*expect_coep_report*/, false /*expect_coep_report_only*/);
  }
}

TEST(CrossOriginResourcePolicyTest, WithCOEPAndDIP) {
  struct TestCase {
    mojom::CrossOriginEmbedderPolicyValue coep_value;
    mojom::CrossOriginEmbedderPolicyValue coep_report_only_value;
    mojom::DocumentIsolationPolicyValue dip_value;
    const std::optional<mojom::BlockedByResponseReason> expected_result;
    bool expect_coep_report;
    bool expect_coep_report_only;
  } test_cases[] = {
      {mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip,
       true, false},
      {mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip,
       true, false},
      {mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByDip,
       false, true},
      {mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip,
       true, true},
      {mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip,
       true, true},
      {mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByDip,
       false, false},
      {mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip,
       true, false},
      {mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip,
       true, false},
      {mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip,
       true, false},
      {mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip,
       true, false},
      {mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByDip,
       false, true},
      {mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip,
       true, true},
      {mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip,
       true, true},
      {mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByDip,
       false, false},
      {mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip,
       true, false},
      {mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip,
       true, false},
  };

  for (const auto& test_case : test_cases) {
    CrossOriginEmbedderPolicy coep;
    coep.value = test_case.coep_value;
    coep.report_only_value = test_case.coep_report_only_value;
    DocumentIsolationPolicy dip(test_case.dip_value);
    RunCORPTestWithCOEPAndDIP(
        coep, dip, test_case.expected_result, true /*include_credentials*/,
        test_case.expect_coep_report, test_case.expect_coep_report_only);
  }
}

TEST(CrossOriginResourcePolicyTest, WithCOEPAndDIPNoCredentials) {
  constexpr auto kAllow = std::nullopt;
  struct TestCase {
    mojom::CrossOriginEmbedderPolicyValue coep_value;
    mojom::CrossOriginEmbedderPolicyValue coep_report_only_value;
    mojom::DocumentIsolationPolicyValue dip_value;
    const std::optional<mojom::BlockedByResponseReason> expected_result;
    bool expect_coep_report;
    bool expect_coep_report_only;
  } test_cases[] = {
      {mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip,
       true, false},
      {mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByDip,
       false, false},
      {mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByDip,
       false, true},
      {mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip,
       true, true},
      {mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByDip,
       false, true},
      {mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByDip,
       false, false},
      {mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip,
       true, false},
      {mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByDip,
       false, false},
      {mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoep,
       true, false},
      {mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless, kAllow,
       false, false},
      {mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless, kAllow,
       false, true},
      {mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoep,
       true, true},
      {mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless, kAllow,
       false, true},
      {mojom::CrossOriginEmbedderPolicyValue::kNone,
       mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless, kAllow,
       false, false},
      {mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
       mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless,
       mojom::BlockedByResponseReason::
           kCorpNotSameOriginAfterDefaultedToSameOriginByCoep,
       true, false},
      {mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
       mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless, kAllow,
       false, false},
  };

  for (const auto& test_case : test_cases) {
    CrossOriginEmbedderPolicy coep;
    coep.value = test_case.coep_value;
    coep.report_only_value = test_case.coep_report_only_value;
    DocumentIsolationPolicy dip(test_case.dip_value);
    RunCORPTestWithCOEPAndDIP(
        coep, dip, test_case.expected_result, false /*include_credentials*/,
        test_case.expect_coep_report, test_case.expect_coep_report_only);
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
