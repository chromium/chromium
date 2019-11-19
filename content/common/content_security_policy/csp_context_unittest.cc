// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "content/common/content_security_policy/csp_context.h"
#include "content/common/content_security_policy_header.h"
#include "content/common/navigation_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class CSPContextTest : public CSPContext {
 public:
  const std::vector<CSPViolationParams>& violations() { return violations_; }

  void AddSchemeToBypassCSP(const std::string& scheme) {
    scheme_to_bypass_.insert(scheme);
  }

  bool SchemeShouldBypassCSP(const base::StringPiece& scheme) override {
    return scheme_to_bypass_.count(scheme.as_string());
  }

  void ClearViolations() { violations_.clear(); }

  void set_sanitize_data_for_use_in_csp_violation(bool value) {
    sanitize_data_for_use_in_csp_violation_ = value;
  }

  void SanitizeDataForUseInCspViolation(
      bool is_redirect,
      CSPDirective::Name directive,
      GURL* blocked_url,
      SourceLocation* source_location) const override {
    if (!sanitize_data_for_use_in_csp_violation_)
      return;
    *blocked_url = blocked_url->GetOrigin();
    *source_location =
        SourceLocation(GURL(source_location->url).GetOrigin().spec(), 0u, 0u);
  }

 private:
  void ReportContentSecurityPolicyViolation(
      const CSPViolationParams& violation_params) override {
    violations_.push_back(violation_params);
  }
  std::vector<CSPViolationParams> violations_;
  std::set<std::string> scheme_to_bypass_;
  bool sanitize_data_for_use_in_csp_violation_ = false;
};

// Build a new policy made of only one directive and no report endpoints.
ContentSecurityPolicy BuildPolicy(CSPDirective::Name directive_name,
                                  std::vector<CSPSource> sources) {
  return ContentSecurityPolicy(
      ContentSecurityPolicyHeader(
          std::string(),  // header
          network::mojom::ContentSecurityPolicyType::kEnforce,
          blink::kWebContentSecurityPolicySourceHTTP),
      {CSPDirective(directive_name,
                    CSPSourceList(false, false, false, sources))},
      std::vector<std::string>(), false);  // report_end_points
}

}  // namespace

TEST(CSPContextTest, SchemeShouldBypassCSP) {
  CSPSource source("", "example.com", false, url::PORT_UNSPECIFIED, false, "");
  CSPContextTest context;
  context.AddContentSecurityPolicy(
      BuildPolicy(CSPDirective::DefaultSrc, {source}));

  EXPECT_FALSE(context.IsAllowedByCsp(
      CSPDirective::FrameSrc, GURL("data:text/html,<html></html>"), false,
      false, SourceLocation(), CSPContext::CHECK_ALL_CSP, false));

  context.AddSchemeToBypassCSP("data");

  EXPECT_TRUE(context.IsAllowedByCsp(
      CSPDirective::FrameSrc, GURL("data:text/html,<html></html>"), false,
      false, SourceLocation(), CSPContext::CHECK_ALL_CSP, false));
}

TEST(CSPContextTest, MultiplePolicies) {
  CSPContextTest context;
  context.SetSelf(url::Origin::Create(GURL("http://example.com")));

  CSPSource source_a("", "a.com", false, url::PORT_UNSPECIFIED, false, "");
  CSPSource source_b("", "b.com", false, url::PORT_UNSPECIFIED, false, "");
  CSPSource source_c("", "c.com", false, url::PORT_UNSPECIFIED, false, "");

  context.AddContentSecurityPolicy(
      BuildPolicy(CSPDirective::FrameSrc, {source_a, source_b}));
  context.AddContentSecurityPolicy(
      BuildPolicy(CSPDirective::FrameSrc, {source_a, source_c}));

  EXPECT_TRUE(context.IsAllowedByCsp(
      CSPDirective::FrameSrc, GURL("http://a.com"), false, false,
      SourceLocation(), CSPContext::CHECK_ALL_CSP, false));
  EXPECT_FALSE(context.IsAllowedByCsp(
      CSPDirective::FrameSrc, GURL("http://b.com"), false, false,
      SourceLocation(), CSPContext::CHECK_ALL_CSP, false));
  EXPECT_FALSE(context.IsAllowedByCsp(
      CSPDirective::FrameSrc, GURL("http://c.com"), false, false,
      SourceLocation(), CSPContext::CHECK_ALL_CSP, false));
  EXPECT_FALSE(context.IsAllowedByCsp(
      CSPDirective::FrameSrc, GURL("http://d.com"), false, false,
      SourceLocation(), CSPContext::CHECK_ALL_CSP, false));
}

TEST(CSPContextTest, SanitizeDataForUseInCspViolation) {
  CSPContextTest context;
  context.SetSelf(url::Origin::Create(GURL("http://a.com")));

  // Content-Security-Policy: frame-src "a.com/iframe"
  context.AddContentSecurityPolicy(
      BuildPolicy(CSPDirective::FrameSrc,
                  {CSPSource("", "a.com", false, url::PORT_UNSPECIFIED, false,
                             "/iframe")}));

  GURL blocked_url("http://a.com/login?password=1234");
  SourceLocation source_location("http://a.com/login", 10u, 20u);

  // When the |blocked_url| and |source_location| aren't sensitive information.
  {
    EXPECT_FALSE(context.IsAllowedByCsp(CSPDirective::FrameSrc, blocked_url,
                                        false, false, source_location,
                                        CSPContext::CHECK_ALL_CSP, false));
    ASSERT_EQ(1u, context.violations().size());
    EXPECT_EQ(context.violations()[0].blocked_url, blocked_url);
    EXPECT_EQ(context.violations()[0].source_location.url,
              "http://a.com/login");
    EXPECT_EQ(context.violations()[0].source_location.line_number, 10u);
    EXPECT_EQ(context.violations()[0].source_location.column_number, 20u);
    EXPECT_EQ(context.violations()[0].console_message,
              "Refused to frame 'http://a.com/login?password=1234' because it "
              "violates the following Content Security Policy directive: "
              "\"frame-src a.com/iframe\".\n");
  }

  context.set_sanitize_data_for_use_in_csp_violation(true);

  // When the |blocked_url| and |source_location| are sensitive information.
  {
    EXPECT_FALSE(context.IsAllowedByCsp(CSPDirective::FrameSrc, blocked_url,
                                        false, false, source_location,
                                        CSPContext::CHECK_ALL_CSP, false));
    ASSERT_EQ(2u, context.violations().size());
    EXPECT_EQ(context.violations()[1].blocked_url, blocked_url.GetOrigin());
    EXPECT_EQ(context.violations()[1].source_location.url, "http://a.com/");
    EXPECT_EQ(context.violations()[1].source_location.line_number, 0u);
    EXPECT_EQ(context.violations()[1].source_location.column_number, 0u);
    EXPECT_EQ(context.violations()[1].console_message,
              "Refused to frame 'http://a.com/' because it violates the "
              "following Content Security Policy directive: \"frame-src "
              "a.com/iframe\".\n");
  }
}

// When several policies are infringed, all of them must be reported.
TEST(CSPContextTest, MultipleInfringement) {
  CSPContextTest context;
  context.SetSelf(url::Origin::Create(GURL("http://example.com")));

  CSPSource source_a("", "a.com", false, url::PORT_UNSPECIFIED, false, "");
  CSPSource source_b("", "b.com", false, url::PORT_UNSPECIFIED, false, "");
  CSPSource source_c("", "c.com", false, url::PORT_UNSPECIFIED, false, "");

  context.AddContentSecurityPolicy(
      BuildPolicy(CSPDirective::FrameSrc, {source_a}));
  context.AddContentSecurityPolicy(
      BuildPolicy(CSPDirective::FrameSrc, {source_b}));
  context.AddContentSecurityPolicy(
      BuildPolicy(CSPDirective::FrameSrc, {source_c}));

  EXPECT_FALSE(context.IsAllowedByCsp(
      CSPDirective::FrameSrc, GURL("http://c.com"), false, false,
      SourceLocation(), CSPContext::CHECK_ALL_CSP, false));
  ASSERT_EQ(2u, context.violations().size());
  const char console_message_a[] =
      "Refused to frame 'http://c.com/' because it violates the following "
      "Content Security Policy directive: \"frame-src a.com\".\n";
  const char console_message_b[] =
      "Refused to frame 'http://c.com/' because it violates the following "
      "Content Security Policy directive: \"frame-src b.com\".\n";
  EXPECT_EQ(console_message_a, context.violations()[0].console_message);
  EXPECT_EQ(console_message_b, context.violations()[1].console_message);
}

// Tests that the CheckCSPDisposition parameter is obeyed.
TEST(CSPContextTest, CheckCSPDisposition) {
  CSPSource source("", "example.com", false, url::PORT_UNSPECIFIED, false, "");
  CSPContextTest context;

  // Add an enforced policy.
  context.AddContentSecurityPolicy(
      BuildPolicy(CSPDirective::FrameSrc, {source}));

  // Add a report-only policy.
  ContentSecurityPolicy report_only =
      BuildPolicy(CSPDirective::DefaultSrc, {source});
  report_only.header.type = network::mojom::ContentSecurityPolicyType::kReport;
  context.AddContentSecurityPolicy(report_only);

  // With CHECK_ALL_CSP, both policies should be checked and violations should
  // be reported.
  EXPECT_FALSE(context.IsAllowedByCsp(
      CSPDirective::FrameSrc, GURL("https://not-example.com"), false, false,
      SourceLocation(), CSPContext::CHECK_ALL_CSP, false));
  ASSERT_EQ(2u, context.violations().size());
  const char console_message_a[] =
      "Refused to frame 'https://not-example.com/' because it violates the "
      "following "
      "Content Security Policy directive: \"frame-src example.com\".\n";
  const char console_message_b[] =
      "[Report Only] Refused to frame 'https://not-example.com/' because it "
      "violates the following "
      "Content Security Policy directive: \"default-src example.com\". Note "
      "that 'frame-src' was not explicitly set, so 'default-src' is used as a "
      "fallback.\n";
  // Both console messages must appear in the reported violations.
  EXPECT_TRUE((console_message_a == context.violations()[0].console_message &&
               console_message_b == context.violations()[1].console_message) ||
              (console_message_a == context.violations()[1].console_message &&
               console_message_b == context.violations()[0].console_message));

  // With CHECK_REPORT_ONLY_CSP, the request should be allowed but reported.
  context.ClearViolations();
  EXPECT_TRUE(context.IsAllowedByCsp(
      CSPDirective::FrameSrc, GURL("https://not-example.com"), false, false,
      SourceLocation(), CSPContext::CHECK_REPORT_ONLY_CSP, false));
  ASSERT_EQ(1u, context.violations().size());
  EXPECT_EQ(console_message_b, context.violations()[0].console_message);

  // With CHECK_ENFORCED_CSP, the request should be blocked and only the
  // enforced policy violation should be reported.
  context.ClearViolations();
  EXPECT_FALSE(context.IsAllowedByCsp(
      CSPDirective::FrameSrc, GURL("https://not-example.com"), false, false,
      SourceLocation(), CSPContext::CHECK_ENFORCED_CSP, false));
  ASSERT_EQ(1u, context.violations().size());
  EXPECT_EQ(console_message_a, context.violations()[0].console_message);
}

// Tests HTTP subresources and form submissions have their URLs upgraded when
// upgrade-insecure-requests is present.
TEST(CSPContextTest, ShouldModifyRequestUrlForCsp) {
  CSPContextTest context;
  context.AddContentSecurityPolicy(BuildPolicy(
      CSPDirective::UpgradeInsecureRequests, std::vector<CSPSource>()));
  // An HTTP subresource or form submission should be upgraded.
  EXPECT_TRUE(context.ShouldModifyRequestUrlForCsp(true));

  // Main-frame navigation requests should not be modified.
  EXPECT_FALSE(context.ShouldModifyRequestUrlForCsp(false));
}

// Tests that URLs passed to ModifyRequestUrlForCsp are modified according to
// the spec for upgrades.
TEST(CSPContextTest, ModifyRequestUrlForCsp) {
  CSPContextTest context;
  GURL test_url;

  test_url = GURL("http://example.com");
  context.ModifyRequestUrlForCsp(&test_url);
  EXPECT_EQ(GURL("https://example.com"), test_url);

  test_url = GURL("http://example.com:80");
  context.ModifyRequestUrlForCsp(&test_url);
  EXPECT_EQ(GURL("https://example.com:443"), test_url);

  // Non-standard ports should not be modified.
  test_url = GURL("http://example-weird-port.com:8088");
  context.ModifyRequestUrlForCsp(&test_url);
  EXPECT_EQ(GURL("https://example-weird-port.com:8088"), test_url);

  // Trusted Non-HTTPS URLs don't need to be modified.
  test_url = GURL("http://127.0.0.1");
  context.ModifyRequestUrlForCsp(&test_url);
  EXPECT_EQ(GURL("http://127.0.0.1"), test_url);

  test_url = GURL("http://127.0.0.8");
  context.ModifyRequestUrlForCsp(&test_url);
  EXPECT_EQ(GURL("http://127.0.0.8"), test_url);

  test_url = GURL("http://localhost");
  context.ModifyRequestUrlForCsp(&test_url);
  EXPECT_EQ(GURL("http://localhost"), test_url);

  test_url = GURL("http://sub.localhost");
  context.ModifyRequestUrlForCsp(&test_url);
  EXPECT_EQ(GURL("http://sub.localhost"), test_url);

  // Non-HTTP URLs don't need to be modified.
  test_url = GURL("https://example.com");
  context.ModifyRequestUrlForCsp(&test_url);
  EXPECT_EQ(GURL("https://example.com"), test_url);

  test_url = GURL("data:text/html,<html></html>");
  context.ModifyRequestUrlForCsp(&test_url);
  EXPECT_EQ(GURL("data:text/html,<html></html>"), test_url);

  test_url = GURL("weird-scheme://this.is.a.url");
  context.ModifyRequestUrlForCsp(&test_url);
  EXPECT_EQ(GURL("weird-scheme://this.is.a.url"), test_url);
}

}  // namespace content
