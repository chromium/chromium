// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/content_security_policy/csp_context.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace network {

using CSPDirectiveName = mojom::CSPDirectiveName;

namespace {

class CSPContextTest : public CSPContext {
 public:
  const std::vector<network::mojom::CSPViolationPtr>& violations() {
    return violations_;
  }

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
      CSPDirectiveName directive,
      GURL* blocked_url,
      network::mojom::SourceLocation* source_location) const override {
    if (!sanitize_data_for_use_in_csp_violation_)
      return;
    *blocked_url = blocked_url->GetOrigin();
    source_location->url = GURL(source_location->url).GetOrigin().spec();
    source_location->line = 0u;
    source_location->column = 0u;
  }

 private:
  void ReportContentSecurityPolicyViolation(
      network::mojom::CSPViolationPtr violation) override {
    violations_.push_back(std::move(violation));
  }
  std::vector<network::mojom::CSPViolationPtr> violations_;
  std::set<std::string> scheme_to_bypass_;
  bool sanitize_data_for_use_in_csp_violation_ = false;
};

mojom::ContentSecurityPolicyPtr EmptyCSP() {
  auto policy = mojom::ContentSecurityPolicy::New();
  policy->header = mojom::ContentSecurityPolicyHeader::New();
  return policy;
}

// Build a new policy made of only one directive and no report endpoints.
mojom::ContentSecurityPolicyPtr BuildPolicy(mojom::CSPSourcePtr self_source,
                                            CSPDirectiveName directive_name,
                                            mojom::CSPSourcePtr source) {
  auto source_list = mojom::CSPSourceList::New();
  source_list->sources.push_back(std::move(source));

  auto policy = EmptyCSP();
  policy->directives[directive_name] = std::move(source_list);
  policy->self_origin = std::move(self_source);

  return policy;
}

// Build a new policy made of only one directive and no report endpoints.
mojom::ContentSecurityPolicyPtr BuildPolicy(mojom::CSPSourcePtr self_source,
                                            CSPDirectiveName directive_name,
                                            mojom::CSPSourcePtr source_1,
                                            mojom::CSPSourcePtr source_2) {
  auto source_list = mojom::CSPSourceList::New();
  source_list->sources.push_back(std::move(source_1));
  source_list->sources.push_back(std::move(source_2));

  auto policy = EmptyCSP();
  policy->directives[directive_name] = std::move(source_list);
  policy->self_origin = std::move(self_source);

  return policy;
}

mojom::CSPSourcePtr BuildCSPSource(const char* scheme, const char* host) {
  return mojom::CSPSource::New(scheme, host, url::PORT_UNSPECIFIED, "", false,
                               false);
}

network::mojom::SourceLocationPtr SourceLocation() {
  return network::mojom::SourceLocation::New();
}

}  // namespace

TEST(CSPContextTest, SchemeShouldBypassCSP) {
  CSPContextTest context;
  auto self_source = network::mojom::CSPSource::New("http", "example.com", 80,
                                                    "", false, false);
  context.AddContentSecurityPolicy(
      BuildPolicy(self_source.Clone(), CSPDirectiveName::DefaultSrc,
                  BuildCSPSource("", "example.com")));

  EXPECT_FALSE(context.IsAllowedByCsp(
      CSPDirectiveName::FrameSrc, GURL("data:text/html,<html></html>"), false,
      false, SourceLocation(), CSPContext::CHECK_ALL_CSP, false));

  context.AddSchemeToBypassCSP("data");

  EXPECT_TRUE(context.IsAllowedByCsp(
      CSPDirectiveName::FrameSrc, GURL("data:text/html,<html></html>"), false,
      false, SourceLocation(), CSPContext::CHECK_ALL_CSP, false));
}

TEST(CSPContextTest, MultiplePolicies) {
  CSPContextTest context;
  auto self_source = network::mojom::CSPSource::New("http", "example.com", 80,
                                                    "", false, false);

  context.AddContentSecurityPolicy(
      BuildPolicy(self_source.Clone(), CSPDirectiveName::FrameSrc,
                  BuildCSPSource("", "a.com"), BuildCSPSource("", "b.com")));
  context.AddContentSecurityPolicy(
      BuildPolicy(self_source.Clone(), CSPDirectiveName::FrameSrc,
                  BuildCSPSource("", "a.com"), BuildCSPSource("", "c.com")));

  EXPECT_TRUE(context.IsAllowedByCsp(
      CSPDirectiveName::FrameSrc, GURL("http://a.com"), false, false,
      SourceLocation(), CSPContext::CHECK_ALL_CSP, false));
  EXPECT_FALSE(context.IsAllowedByCsp(
      CSPDirectiveName::FrameSrc, GURL("http://b.com"), false, false,
      SourceLocation(), CSPContext::CHECK_ALL_CSP, false));
  EXPECT_FALSE(context.IsAllowedByCsp(
      CSPDirectiveName::FrameSrc, GURL("http://c.com"), false, false,
      SourceLocation(), CSPContext::CHECK_ALL_CSP, false));
  EXPECT_FALSE(context.IsAllowedByCsp(
      CSPDirectiveName::FrameSrc, GURL("http://d.com"), false, false,
      SourceLocation(), CSPContext::CHECK_ALL_CSP, false));
}

TEST(CSPContextTest, SanitizeDataForUseInCspViolation) {
  CSPContextTest context;
  auto self_source =
      network::mojom::CSPSource::New("http", "a.com", 80, "", false, false);

  // Content-Security-Policy: frame-src "a.com/iframe"
  context.AddContentSecurityPolicy(
      BuildPolicy(self_source.Clone(), CSPDirectiveName::FrameSrc,
                  mojom::CSPSource::New("", "a.com", url::PORT_UNSPECIFIED,
                                        "/iframe", false, false)));

  GURL blocked_url("http://a.com/login?password=1234");
  auto source_location =
      network::mojom::SourceLocation::New("http://a.com/login", 10u, 20u);

  // When the |blocked_url| and |source_location| aren't sensitive information.
  {
    EXPECT_FALSE(context.IsAllowedByCsp(CSPDirectiveName::FrameSrc, blocked_url,
                                        false, false, source_location,
                                        CSPContext::CHECK_ALL_CSP, false));
    ASSERT_EQ(1u, context.violations().size());
    EXPECT_EQ(context.violations()[0]->blocked_url, blocked_url);
    EXPECT_EQ(context.violations()[0]->source_location->url,
              "http://a.com/login");
    EXPECT_EQ(context.violations()[0]->source_location->line, 10u);
    EXPECT_EQ(context.violations()[0]->source_location->column, 20u);
    EXPECT_EQ(context.violations()[0]->console_message,
              "Refused to frame 'http://a.com/login?password=1234' because it "
              "violates the following Content Security Policy directive: "
              "\"frame-src a.com/iframe\".\n");
  }

  context.set_sanitize_data_for_use_in_csp_violation(true);

  // When the |blocked_url| and |source_location| are sensitive information.
  {
    EXPECT_FALSE(context.IsAllowedByCsp(CSPDirectiveName::FrameSrc, blocked_url,
                                        false, false, source_location,
                                        CSPContext::CHECK_ALL_CSP, false));
    ASSERT_EQ(2u, context.violations().size());
    EXPECT_EQ(context.violations()[1]->blocked_url, blocked_url.GetOrigin());
    EXPECT_EQ(context.violations()[1]->source_location->url, "http://a.com/");
    EXPECT_EQ(context.violations()[1]->source_location->line, 0u);
    EXPECT_EQ(context.violations()[1]->source_location->column, 0u);
    EXPECT_EQ(context.violations()[1]->console_message,
              "Refused to frame 'http://a.com/' because it violates the "
              "following Content Security Policy directive: \"frame-src "
              "a.com/iframe\".\n");
  }
}

// When several policies are infringed, all of them must be reported.
TEST(CSPContextTest, MultipleInfringement) {
  CSPContextTest context;
  auto self_source = network::mojom::CSPSource::New("http", "example.com", 80,
                                                    "", false, false);

  context.AddContentSecurityPolicy(BuildPolicy(self_source.Clone(),
                                               CSPDirectiveName::FrameSrc,
                                               BuildCSPSource("", "a.com")));
  context.AddContentSecurityPolicy(BuildPolicy(self_source.Clone(),
                                               CSPDirectiveName::FrameSrc,
                                               BuildCSPSource("", "b.com")));
  context.AddContentSecurityPolicy(BuildPolicy(self_source.Clone(),
                                               CSPDirectiveName::FrameSrc,
                                               BuildCSPSource("", "c.com")));

  EXPECT_FALSE(context.IsAllowedByCsp(
      CSPDirectiveName::FrameSrc, GURL("http://c.com"), false, false,
      SourceLocation(), CSPContext::CHECK_ALL_CSP, false));
  ASSERT_EQ(2u, context.violations().size());
  const char console_message_a[] =
      "Refused to frame 'http://c.com/' because it violates the following "
      "Content Security Policy directive: \"frame-src a.com\".\n";
  const char console_message_b[] =
      "Refused to frame 'http://c.com/' because it violates the following "
      "Content Security Policy directive: \"frame-src b.com\".\n";
  EXPECT_EQ(console_message_a, context.violations()[0]->console_message);
  EXPECT_EQ(console_message_b, context.violations()[1]->console_message);
}

// Tests that the CheckCSPDisposition parameter is obeyed.
TEST(CSPContextTest, CheckCSPDisposition) {
  CSPContextTest context;
  auto self_source = network::mojom::CSPSource::New("http", "example.com", 80,
                                                    "", false, false);

  // Add an enforced policy.
  auto enforce_csp =
      BuildPolicy(self_source.Clone(), CSPDirectiveName::FrameSrc,
                  BuildCSPSource("", "example.com"));
  // Add a report-only policy.
  auto report_only_csp =
      BuildPolicy(self_source.Clone(), CSPDirectiveName::DefaultSrc,
                  BuildCSPSource("", "example.com"));
  report_only_csp->header->type = mojom::ContentSecurityPolicyType::kReport;

  context.AddContentSecurityPolicy(std::move(enforce_csp));
  context.AddContentSecurityPolicy(std::move(report_only_csp));

  // With CHECK_ALL_CSP, both policies should be checked and violations should
  // be reported.
  EXPECT_FALSE(context.IsAllowedByCsp(
      CSPDirectiveName::FrameSrc, GURL("https://not-example.com"), false, false,
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
  EXPECT_TRUE((console_message_a == context.violations()[0]->console_message &&
               console_message_b == context.violations()[1]->console_message) ||
              (console_message_a == context.violations()[1]->console_message &&
               console_message_b == context.violations()[0]->console_message));

  // With CHECK_REPORT_ONLY_CSP, the request should be allowed but reported.
  context.ClearViolations();
  EXPECT_TRUE(context.IsAllowedByCsp(
      CSPDirectiveName::FrameSrc, GURL("https://not-example.com"), false, false,
      SourceLocation(), CSPContext::CHECK_REPORT_ONLY_CSP, false));
  ASSERT_EQ(1u, context.violations().size());
  EXPECT_EQ(console_message_b, context.violations()[0]->console_message);

  // With CHECK_ENFORCED_CSP, the request should be blocked and only the
  // enforced policy violation should be reported.
  context.ClearViolations();
  EXPECT_FALSE(context.IsAllowedByCsp(
      CSPDirectiveName::FrameSrc, GURL("https://not-example.com"), false, false,
      SourceLocation(), CSPContext::CHECK_ENFORCED_CSP, false));
  ASSERT_EQ(1u, context.violations().size());
  EXPECT_EQ(console_message_a, context.violations()[0]->console_message);
}

}  // namespace network
