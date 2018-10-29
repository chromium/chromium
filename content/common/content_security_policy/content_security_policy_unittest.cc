// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util.h"
#include "content/common/content_security_policy/csp_context.h"
#include "content/common/content_security_policy_header.h"
#include "content/common/navigation_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
class CSPContextTest : public CSPContext {
 public:
  CSPContextTest() : CSPContext() {}

  const std::vector<CSPViolationParams>& violations() { return violations_; }

  void AddSchemeToBypassCSP(const std::string& scheme) {
    scheme_to_bypass_.push_back(scheme);
  }

  bool SchemeShouldBypassCSP(const base::StringPiece& scheme) override {
    return base::ContainsValue(scheme_to_bypass_, scheme);
  }

 private:
  void ReportContentSecurityPolicyViolation(
      const CSPViolationParams& violation_params) override {
    violations_.push_back(violation_params);
  }
  std::vector<CSPViolationParams> violations_;
  std::vector<std::string> scheme_to_bypass_;

  DISALLOW_COPY_AND_ASSIGN(CSPContextTest);
};

ContentSecurityPolicyHeader EmptyCspHeader() {
  return ContentSecurityPolicyHeader(
      std::string(), blink::kWebContentSecurityPolicyTypeEnforce,
      blink::kWebContentSecurityPolicySourceHTTP);
}

}  // namespace

TEST(ContentSecurityPolicy, NoDirective) {
  CSPContextTest context;
  std::vector<std::string> report_end_points;  // empty
  ContentSecurityPolicy policy(EmptyCspHeader(), std::vector<CSPDirective>(),
                               report_end_points, false);

  EXPECT_TRUE(ContentSecurityPolicy::Allow(
      policy, CSPDirective::FormAction, GURL("http://www.example.com"), false,
      false, &context, SourceLocation(), true));
  ASSERT_EQ(0u, context.violations().size());
}

TEST(ContentSecurityPolicy, ReportViolation) {
  CSPContextTest context;

  // source = "www.example.com"
  CSPSource source("", "www.example.com", false, url::PORT_UNSPECIFIED, false,
                   "");
  CSPSourceList source_list(false, false, false, {source});
  CSPDirective directive(CSPDirective::FormAction, source_list);
  std::vector<std::string> report_end_points;  // empty
  ContentSecurityPolicy policy(EmptyCspHeader(), {directive}, report_end_points,
                               false);

  EXPECT_FALSE(ContentSecurityPolicy::Allow(
      policy, CSPDirective::FormAction, GURL("http://www.not-example.com"),
      false, false, &context, SourceLocation(), true));

  ASSERT_EQ(1u, context.violations().size());
  const char console_message[] =
      "Refused to send form data to 'http://www.not-example.com/' because it "
      "violates the following Content Security Policy directive: \"form-action "
      "www.example.com\".\n";
  EXPECT_EQ(console_message, context.violations()[0].console_message);
}

TEST(ContentSecurityPolicy, DirectiveFallback) {
  CSPSource source_a("http", "a.com", false, url::PORT_UNSPECIFIED, false, "");
  CSPSource source_b("http", "b.com", false, url::PORT_UNSPECIFIED, false, "");
  CSPSourceList source_list_a(false, false, false, {source_a});
  CSPSourceList source_list_b(false, false, false, {source_b});

  std::vector<std::string> report_end_points;  // Empty.

  {
    CSPContextTest context;
    ContentSecurityPolicy policy(
        EmptyCspHeader(),
        {CSPDirective(CSPDirective::DefaultSrc, source_list_a)},
        report_end_points, false);
    EXPECT_FALSE(ContentSecurityPolicy::Allow(
        policy, CSPDirective::FrameSrc, GURL("http://b.com"), false, false,
        &context, SourceLocation(), false));
    ASSERT_EQ(1u, context.violations().size());
    const char console_message[] =
        "Refused to frame 'http://b.com/' because it violates "
        "the following Content Security Policy directive: \"default-src "
        "http://a.com\". Note that 'frame-src' was not explicitly "
        "set, so 'default-src' is used as a fallback.\n";
    EXPECT_EQ(console_message, context.violations()[0].console_message);
    EXPECT_TRUE(ContentSecurityPolicy::Allow(
        policy, CSPDirective::FrameSrc, GURL("http://a.com"), false, false,
        &context, SourceLocation(), false));
  }
  {
    CSPContextTest context;
    ContentSecurityPolicy policy(
        EmptyCspHeader(), {CSPDirective(CSPDirective::ChildSrc, source_list_a)},
        report_end_points, false);
    EXPECT_FALSE(ContentSecurityPolicy::Allow(
        policy, CSPDirective::FrameSrc, GURL("http://b.com"), false, false,
        &context, SourceLocation(), false));
    ASSERT_EQ(1u, context.violations().size());
    const char console_message[] =
        "Refused to frame 'http://b.com/' because it violates "
        "the following Content Security Policy directive: \"child-src "
        "http://a.com\". Note that 'frame-src' was not explicitly "
        "set, so 'child-src' is used as a fallback.\n";
    EXPECT_EQ(console_message, context.violations()[0].console_message);
    EXPECT_TRUE(ContentSecurityPolicy::Allow(
        policy, CSPDirective::FrameSrc, GURL("http://a.com"), false, false,
        &context, SourceLocation(), false));
  }
  {
    CSPContextTest context;
    CSPSourceList source_list(false, false, false, {source_a, source_b});
    ContentSecurityPolicy policy(
        EmptyCspHeader(),
        {CSPDirective(CSPDirective::FrameSrc, {source_list_a}),
         CSPDirective(CSPDirective::ChildSrc, {source_list_b})},
        report_end_points, false);
    EXPECT_TRUE(ContentSecurityPolicy::Allow(
        policy, CSPDirective::FrameSrc, GURL("http://a.com"), false, false,
        &context, SourceLocation(), false));
    EXPECT_FALSE(ContentSecurityPolicy::Allow(
        policy, CSPDirective::FrameSrc, GURL("http://b.com"), false, false,
        &context, SourceLocation(), false));
    ASSERT_EQ(1u, context.violations().size());
    const char console_message[] =
        "Refused to frame 'http://b.com/' because it violates "
        "the following Content Security Policy directive: \"frame-src "
        "http://a.com\".\n";
    EXPECT_EQ(console_message, context.violations()[0].console_message);
  }
}

TEST(ContentSecurityPolicy, RequestsAllowedWhenBypassingCSP) {
  CSPContextTest context;
  std::vector<std::string> report_end_points;  // empty
  CSPSource source("https", "example.com", false, url::PORT_UNSPECIFIED, false,
                   "");
  CSPSourceList source_list(false, false, false, {source});
  ContentSecurityPolicy policy(
      EmptyCspHeader(), {CSPDirective(CSPDirective::DefaultSrc, source_list)},
      report_end_points, false);

  EXPECT_TRUE(ContentSecurityPolicy::Allow(
      policy, CSPDirective::FrameSrc, GURL("https://example.com/"), false,
      false, &context, SourceLocation(), false));
  EXPECT_FALSE(ContentSecurityPolicy::Allow(
      policy, CSPDirective::FrameSrc, GURL("https://not-example.com/"), false,
      false, &context, SourceLocation(), false));

  // Register 'https' as bypassing CSP, which should now bypass is entirely.
  context.AddSchemeToBypassCSP("https");

  EXPECT_TRUE(ContentSecurityPolicy::Allow(
      policy, CSPDirective::FrameSrc, GURL("https://example.com/"), false,
      false, &context, SourceLocation(), false));
  EXPECT_TRUE(ContentSecurityPolicy::Allow(
      policy, CSPDirective::FrameSrc, GURL("https://not-example.com/"), false,
      false, &context, SourceLocation(), false));
}

TEST(ContentSecurityPolicy, FilesystemAllowedWhenBypassingCSP) {
  CSPContextTest context;
  std::vector<std::string> report_end_points;  // empty
  CSPSource source("https", "example.com", false, url::PORT_UNSPECIFIED, false,
                   "");
  CSPSourceList source_list(false, false, false, {source});
  ContentSecurityPolicy policy(
      EmptyCspHeader(), {CSPDirective(CSPDirective::DefaultSrc, source_list)},
      report_end_points, false);

  EXPECT_FALSE(ContentSecurityPolicy::Allow(
      policy, CSPDirective::FrameSrc,
      GURL("filesystem:https://example.com/file.txt"), false, false, &context,
      SourceLocation(), false));
  EXPECT_FALSE(ContentSecurityPolicy::Allow(
      policy, CSPDirective::FrameSrc,
      GURL("filesystem:https://not-example.com/file.txt"), false, false,
      &context, SourceLocation(), false));

  // Register 'https' as bypassing CSP, which should now bypass is entirely.
  context.AddSchemeToBypassCSP("https");

  EXPECT_TRUE(ContentSecurityPolicy::Allow(
      policy, CSPDirective::FrameSrc,
      GURL("filesystem:https://example.com/file.txt"), false, false, &context,
      SourceLocation(), false));
  EXPECT_TRUE(ContentSecurityPolicy::Allow(
      policy, CSPDirective::FrameSrc,
      GURL("filesystem:https://not-example.com/file.txt"), false, false,
      &context, SourceLocation(), false));
}

TEST(ContentSecurityPolicy, BlobAllowedWhenBypassingCSP) {
  CSPContextTest context;
  std::vector<std::string> report_end_points;  // empty
  CSPSource source("https", "example.com", false, url::PORT_UNSPECIFIED, false,
                   "");
  CSPSourceList source_list(false, false, false, {source});
  ContentSecurityPolicy policy(
      EmptyCspHeader(), {CSPDirective(CSPDirective::DefaultSrc, source_list)},
      report_end_points, false);

  EXPECT_FALSE(ContentSecurityPolicy::Allow(
      policy, CSPDirective::FrameSrc, GURL("blob:https://example.com/"), false,
      false, &context, SourceLocation(), false));
  EXPECT_FALSE(ContentSecurityPolicy::Allow(
      policy, CSPDirective::FrameSrc, GURL("blob:https://not-example.com/"),
      false, false, &context, SourceLocation(), false));

  // Register 'https' as bypassing CSP, which should now bypass is entirely.
  context.AddSchemeToBypassCSP("https");

  EXPECT_TRUE(ContentSecurityPolicy::Allow(
      policy, CSPDirective::FrameSrc, GURL("blob:https://example.com/"), false,
      false, &context, SourceLocation(), false));
  EXPECT_TRUE(ContentSecurityPolicy::Allow(
      policy, CSPDirective::FrameSrc, GURL("blob:https://not-example.com/"),
      false, false, &context, SourceLocation(), false));
}

TEST(ContentSecurityPolicy, ShouldUpgradeInsecureRequest) {
  std::vector<std::string> report_end_points;  // empty
  CSPSource source("https", "example.com", false, url::PORT_UNSPECIFIED, false,
                   "");
  CSPSourceList source_list(false, false, false, {source});
  ContentSecurityPolicy policy(
      EmptyCspHeader(), {CSPDirective(CSPDirective::DefaultSrc, source_list)},
      report_end_points, false);

  EXPECT_FALSE(ContentSecurityPolicy::ShouldUpgradeInsecureRequest(policy));

  policy.directives.push_back(
      CSPDirective(CSPDirective::UpgradeInsecureRequests, CSPSourceList()));
  EXPECT_TRUE(ContentSecurityPolicy::ShouldUpgradeInsecureRequest(policy));
}

TEST(ContentSecurityPolicy, NavigateToChecks) {
  CSPContextTest context;
  std::vector<std::string> report_end_points;  // empty
  CSPSource example("https", "example.test", false, url::PORT_UNSPECIFIED,
                    false, "");
  CSPSourceList none_source_list(false, false, false, {});
  CSPSourceList example_source_list(false, false, false, {example});
  CSPSourceList self_source_list(true, false, false, {});
  CSPSourceList redirects_source_list(false, false, true, {});
  CSPSourceList redirects_example_source_list(false, false, true, {example});
  context.SetSelf(example);

  struct TestCase {
    const CSPSourceList& navigate_to_list;
    const GURL& url;
    bool is_response_check;
    bool expected;
    bool is_form_submission;
    const CSPSourceList* form_action_list;
  } cases[] = {
      // Basic source matching.
      {none_source_list, GURL("https://example.test"), false, false, false,
       nullptr},
      {example_source_list, GURL("https://example.test"), false, true, false,
       nullptr},
      {example_source_list, GURL("https://not-example.test"), false, false,
       false, nullptr},
      {self_source_list, GURL("https://example.test"), false, true, false,
       nullptr},

      // Checking allow_redirect flag interactions.
      {redirects_source_list, GURL("https://example.test"), false, true, false,
       nullptr},
      {redirects_source_list, GURL("https://example.test"), true, false, false,
       nullptr},
      {redirects_example_source_list, GURL("https://example.test"), false, true,
       false, nullptr},
      {redirects_example_source_list, GURL("https://example.test"), true, true,
       false, nullptr},

      // Interaction with form-action

      // Form submission without form-action present
      {none_source_list, GURL("https://example.test"), false, false, true,
       nullptr},
      {example_source_list, GURL("https://example.test"), false, true, true,
       nullptr},
      {example_source_list, GURL("https://not-example.test"), false, false,
       true, nullptr},
      {self_source_list, GURL("https://example.test"), false, true, true,
       nullptr},

      // Form submission with form-action present
      {none_source_list, GURL("https://example.test"), false, true, true,
       &example_source_list},
      {example_source_list, GURL("https://example.test"), false, true, true,
       &example_source_list},
      {example_source_list, GURL("https://not-example.test"), false, true, true,
       &example_source_list},
      {self_source_list, GURL("https://example.test"), false, true, true,
       &example_source_list},
  };

  for (const auto& test : cases) {
    std::vector<CSPDirective> directives;
    directives.push_back(
        CSPDirective(CSPDirective::NavigateTo, test.navigate_to_list));

    if (test.form_action_list)
      directives.push_back(
          CSPDirective(CSPDirective::FormAction, *(test.form_action_list)));

    ContentSecurityPolicy policy(EmptyCspHeader(), directives,
                                 report_end_points, false);

    EXPECT_EQ(test.expected, ContentSecurityPolicy::Allow(
                                 policy, CSPDirective::NavigateTo, test.url,
                                 true, test.is_response_check, &context,
                                 SourceLocation(), test.is_form_submission));
    EXPECT_EQ(test.expected, ContentSecurityPolicy::Allow(
                                 policy, CSPDirective::NavigateTo, test.url,
                                 false, test.is_response_check, &context,
                                 SourceLocation(), test.is_form_submission));
  }
}

}  // namespace content
