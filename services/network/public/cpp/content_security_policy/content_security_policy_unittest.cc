// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_security_policy/content_security_policy.h"

#include "base/stl_util.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/content_security_policy/csp_context.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/third_party/mozilla/url_parse.h"

namespace network {

using CSPDirectiveName = mojom::CSPDirectiveName;

namespace {

struct ExpectedResult {
  struct ParsedSource {
    std::string scheme;
    std::string host;
    int port = url::PORT_UNSPECIFIED;
    std::string path = "";
    bool is_host_wildcard = false;
    bool is_port_wildcard = false;
  };
  std::vector<ParsedSource> parsed_sources;
  bool allow_self = false;
  bool allow_star = false;
};

struct TestData {
  std::string header;
  ExpectedResult expected_result = ExpectedResult();
};

std::vector<mojom::ContentSecurityPolicyPtr> ParseCSP(std::string expression) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  headers->SetHeader("Content-Security-Policy", expression);
  std::vector<mojom::ContentSecurityPolicyPtr> policies;
  AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                      &policies);
  return policies;
}

static void TestFrameAncestorsCSPParser(const std::string& header,
                                        const ExpectedResult* expected_result) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  headers->SetHeader("Content-Security-Policy", "frame-ancestors " + header);
  std::vector<mojom::ContentSecurityPolicyPtr> policies;
  AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                      &policies);

  auto& frame_ancestors =
      policies[0]->directives[mojom::CSPDirectiveName::FrameAncestors];
  EXPECT_EQ(frame_ancestors->sources.size(),
            expected_result->parsed_sources.size());
  for (size_t i = 0; i < expected_result->parsed_sources.size(); i++) {
    EXPECT_EQ(frame_ancestors->sources[i]->scheme,
              expected_result->parsed_sources[i].scheme);
    EXPECT_EQ(frame_ancestors->sources[i]->host,
              expected_result->parsed_sources[i].host);
    EXPECT_EQ(frame_ancestors->sources[i]->port,
              expected_result->parsed_sources[i].port);
    EXPECT_EQ(frame_ancestors->sources[i]->path,
              expected_result->parsed_sources[i].path);
    EXPECT_EQ(frame_ancestors->sources[i]->is_host_wildcard,
              expected_result->parsed_sources[i].is_host_wildcard);
    EXPECT_EQ(frame_ancestors->sources[i]->is_port_wildcard,
              expected_result->parsed_sources[i].is_port_wildcard);
  }
  EXPECT_EQ(frame_ancestors->allow_self, expected_result->allow_self);
  EXPECT_EQ(frame_ancestors->allow_star, expected_result->allow_star);
}

class CSPContextTest : public CSPContext {
 public:
  CSPContextTest() = default;

  const std::vector<network::mojom::CSPViolationPtr>& violations() {
    return violations_;
  }

  void AddSchemeToBypassCSP(const std::string& scheme) {
    scheme_to_bypass_.push_back(scheme);
  }

  bool SchemeShouldBypassCSP(const base::StringPiece& scheme) override {
    return base::Contains(scheme_to_bypass_, scheme);
  }

 private:
  void ReportContentSecurityPolicyViolation(
      network::mojom::CSPViolationPtr violation) override {
    violations_.push_back(std::move(violation));
  }
  std::vector<network::mojom::CSPViolationPtr> violations_;
  std::vector<std::string> scheme_to_bypass_;

  DISALLOW_COPY_AND_ASSIGN(CSPContextTest);
};

mojom::ContentSecurityPolicyPtr EmptyCSP() {
  auto policy = mojom::ContentSecurityPolicy::New();
  policy->header = mojom::ContentSecurityPolicyHeader::New();
  return policy;
}

// Build a new policy made of only one directive and no report endpoints.
mojom::ContentSecurityPolicyPtr BuildPolicy(CSPDirectiveName directive_name,
                                            mojom::CSPSourcePtr source) {
  auto source_list = mojom::CSPSourceList::New();
  source_list->sources.push_back(std::move(source));

  auto policy = EmptyCSP();
  policy->directives[directive_name] = std::move(source_list);

  return policy;
}

mojom::CSPSourcePtr BuildCSPSource(const char* scheme, const char* host) {
  return mojom::CSPSource::New(scheme, host, url::PORT_UNSPECIFIED, "", false,
                               false);
}

// Return "Content-Security-Policy: default-src <host>"
mojom::ContentSecurityPolicyPtr DefaultSrc(const char* scheme,
                                           const char* host) {
  return BuildPolicy(CSPDirectiveName::DefaultSrc,
                     BuildCSPSource(scheme, host));
}

network::mojom::SourceLocationPtr SourceLocation() {
  return network::mojom::SourceLocation::New();
}

}  // namespace

TEST(ContentSecurityPolicy, ParseFrameAncestors) {
  TestData test_data[] = {
      // Parse scheme.
      // Empty scheme.
      {":"},

      // First character is alpha/non-alpha.
      {"a:", {{{"a", ""}}}},
      {"1ba:"},
      {"-:"},

      // Remaining characters.
      {"abcd:", {{{"abcd", ""}}}},
      {"a123:", {{{"a123", ""}}}},
      {"a+-:", {{{"a+-", ""}}}},
      {"a1+-:", {{{"a1+-", ""}}}},

      // Invalid character.
      {"wrong_scheme"},
      {"wrong_scheme://"},

      // Parse host.
      {"*."},
      {"*.a", {{{"", "a", url::PORT_UNSPECIFIED, "", true, false}}}},
      {"a.*"},
      {"a.*.b"},
      {"*a"},

      // Dot separation.
      {"a", {{{"", "a"}}}},
      {"a.b.c", {{{"", "a.b.c"}}}},
      {"a.b."},
      {".b.c"},
      {"a..c"},

      // Valid/Invalid characters.
      {"az09-", {{{"", "az09-"}}}},
      {"+"},

      // Strange host.
      {"---.com", {{{"", "---.com"}}}},

      // Parse port.
      // Empty port.
      {"scheme://host:"},

      // Common case.
      {"a:80", {{{"", "a", 80, ""}}}},

      // Wildcard port.
      {"a:*", {{{"", "a", url::PORT_UNSPECIFIED, "", false, true}}}},

      // Leading zeroes.
      {"a:000", {{{"", "a", 0, ""}}}},
      {"a:0", {{{"", "a", 0, ""}}}},

      // Invalid chars.
      {"a:-1"},
      {"a:+1"},

      // Parse path.
      // Encoded.
      {"example.com/%48%65%6c%6c%6f%20%57%6f%72%6c%64",
       {{{"", "example.com", url::PORT_UNSPECIFIED, "/Hello World"}}}},

      // Special keyword.
      {"'none'", {{}, false, false}},
      {"'self'", {{}, true, false}},
      {"*", {{}, false, true}},

      // Invalid 'none'. This is an invalid expression according to the CSP
      // grammar, but it is accepted because the parser ignores individual
      // invalid source-expressions.
      {"example.com 'none'", {{{"", "example.com"}}}},

      // Other.
      {"*:*", {{{"", "", url::PORT_UNSPECIFIED, "", true, true}}}},
      {"http:", {{{"http", ""}}}},
      {"https://*", {{{"https", "", url::PORT_UNSPECIFIED, "", true}}}},
      {"http:/example.com"},
      {"http://"},
      {"example.com", {{{"", "example.com"}}}},
      {"example.com/path",
       {{{"", "example.com", url::PORT_UNSPECIFIED, "/path"}}}},
      {"https://example.com", {{{"https", "example.com"}}}},
      {"https://example.com/path",
       {{{"https", "example.com", url::PORT_UNSPECIFIED, "/path"}}}},
      {"https://example.com:1234", {{{"https", "example.com", 1234, ""}}}},
      {"https://example.com:2345/some/path",
       {{{"https", "example.com", 2345, "/some/path"}}}},
      {"example.com example.org", {{{"", "example.com"}, {"", "example.org"}}}},
      {"example.com\texample.org",
       {{{"", "example.com"}, {"", "example.org"}}}},
      {"about:blank"},
      {""},
  };

  for (auto& test : test_data)
    TestFrameAncestorsCSPParser(test.header, &test.expected_result);
}

TEST(ContentSecurityPolicy, ParseDirectives) {
  // One duplicate directive.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader("Content-Security-Policy",
                       "frame-ancestors example.com; script-src "
                       "example2.com; frame-ancestors example3.com");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);
    EXPECT_EQ(2U, policies[0]->directives.size());

    auto& frame_ancestors =
        policies[0]->directives[mojom::CSPDirectiveName::FrameAncestors];
    EXPECT_EQ(frame_ancestors->sources.size(), 1U);
    EXPECT_EQ(frame_ancestors->sources[0]->scheme, "");
    EXPECT_EQ(frame_ancestors->sources[0]->host, "example.com");
    EXPECT_EQ(frame_ancestors->sources[0]->port, url::PORT_UNSPECIFIED);
    EXPECT_EQ(frame_ancestors->sources[0]->path, "");
    EXPECT_EQ(frame_ancestors->sources[0]->is_host_wildcard, false);
    EXPECT_EQ(frame_ancestors->sources[0]->is_port_wildcard, false);
    EXPECT_EQ(frame_ancestors->allow_self, false);

    auto& script_src =
        policies[0]->directives[mojom::CSPDirectiveName::ScriptSrc];
    EXPECT_EQ(script_src->sources.size(), 1U);
    EXPECT_EQ(script_src->sources[0]->scheme, "");
    EXPECT_EQ(script_src->sources[0]->host, "example2.com");
    EXPECT_EQ(script_src->sources[0]->port, url::PORT_UNSPECIFIED);
    EXPECT_EQ(script_src->sources[0]->path, "");
    EXPECT_EQ(script_src->sources[0]->is_host_wildcard, false);
    EXPECT_EQ(script_src->sources[0]->is_port_wildcard, false);
    EXPECT_EQ(script_src->allow_self, false);

    EXPECT_EQ(1U, policies[0]->parsing_errors.size());
    EXPECT_EQ(
        "Ignoring duplicate Content-Security-Policy directive "
        "'frame-ancestors'.",
        policies[0]->parsing_errors[0]);
  }

  // One invalid directive.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader("Content-Security-Policy",
                       "other-directive value; frame-ancestors "
                       "example.org");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);
    EXPECT_EQ(1U, policies[0]->directives.size());

    auto& frame_ancestors =
        policies[0]->directives[mojom::CSPDirectiveName::FrameAncestors];
    EXPECT_EQ(frame_ancestors->sources.size(), 1U);
    EXPECT_EQ(frame_ancestors->sources[0]->scheme, "");
    EXPECT_EQ(frame_ancestors->sources[0]->host, "example.org");
    EXPECT_EQ(frame_ancestors->sources[0]->port, url::PORT_UNSPECIFIED);
    EXPECT_EQ(frame_ancestors->sources[0]->path, "");
    EXPECT_EQ(frame_ancestors->sources[0]->is_host_wildcard, false);
    EXPECT_EQ(frame_ancestors->sources[0]->is_port_wildcard, false);
    EXPECT_EQ(frame_ancestors->allow_self, false);
    EXPECT_EQ(frame_ancestors->allow_star, false);

    EXPECT_EQ(1U, policies[0]->parsing_errors.size());
    EXPECT_EQ(
        "Unrecognized Content-Security-Policy directive 'other-directive'.",
        policies[0]->parsing_errors[0]);
  }

  // Invalid characters in directive name.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader("Content-Security-Policy",
                       "frame_ancestors example.com;");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);
    EXPECT_TRUE(policies[0]->directives.empty());

    EXPECT_EQ(1U, policies[0]->parsing_errors.size());
    EXPECT_EQ(
        "The Content-Security-Policy directive name 'frame_ancestors' contains "
        "one or more invalid characters. Only ASCII alphanumeric characters or "
        "dashes '-' are allowed in directive names.",
        policies[0]->parsing_errors[0]);
  }

  // Invalid characters in directive value.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader("Content-Security-Policy", "frame-ancestors ü.com;");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);
    EXPECT_TRUE(policies[0]->directives.empty());

    EXPECT_EQ(1U, policies[0]->parsing_errors.size());
    EXPECT_EQ(
        "The value for the Content-Security-Policy directive 'frame-ancestors' "
        "contains one or more invalid characters. Non-whitespace characters "
        "outside ASCII 0x21-0x7E must be percent-encoded, as described in RFC "
        "3986, section 2.1: http://tools.ietf.org/html/rfc3986#section-2.1.",
        policies[0]->parsing_errors[0]);
  }

  // Missing semicolon between directive names.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader("Content-Security-Policy", "frame-ancestors object-src");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);
    EXPECT_EQ(1U, policies[0]->directives.size());

    auto& frame_ancestors =
        policies[0]->directives[mojom::CSPDirectiveName::FrameAncestors];
    EXPECT_EQ(frame_ancestors->sources.size(), 1U);
    EXPECT_EQ(frame_ancestors->sources[0]->scheme, "");
    EXPECT_EQ(frame_ancestors->sources[0]->host, "object-src");
    EXPECT_EQ(frame_ancestors->sources[0]->port, url::PORT_UNSPECIFIED);
    EXPECT_EQ(frame_ancestors->sources[0]->path, "");
    EXPECT_EQ(frame_ancestors->sources[0]->is_host_wildcard, false);
    EXPECT_EQ(frame_ancestors->sources[0]->is_port_wildcard, false);
    EXPECT_EQ(frame_ancestors->allow_self, false);
    EXPECT_EQ(frame_ancestors->allow_star, false);

    EXPECT_EQ(1U, policies[0]->parsing_errors.size());
    EXPECT_EQ(
        "The Content-Security-Policy directive 'frame-ancestors' contains "
        "'object-src' as a source expression. Did you want to add it as a "
        "directive and forget a semicolon?",
        policies[0]->parsing_errors[0]);
  }

  // Path containing query.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader("Content-Security-Policy",
                       "frame-ancestors http://example.org/index.html?a=b;");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);
    EXPECT_EQ(1U, policies[0]->directives.size());

    auto& frame_ancestors =
        policies[0]->directives[mojom::CSPDirectiveName::FrameAncestors];
    EXPECT_EQ(frame_ancestors->sources.size(), 1U);
    EXPECT_EQ(frame_ancestors->sources[0]->scheme, "http");
    EXPECT_EQ(frame_ancestors->sources[0]->host, "example.org");
    EXPECT_EQ(frame_ancestors->sources[0]->port, url::PORT_UNSPECIFIED);
    EXPECT_EQ(frame_ancestors->sources[0]->path, "/index.html");
    EXPECT_EQ(frame_ancestors->sources[0]->is_host_wildcard, false);
    EXPECT_EQ(frame_ancestors->sources[0]->is_port_wildcard, false);
    EXPECT_EQ(frame_ancestors->allow_self, false);
    EXPECT_EQ(frame_ancestors->allow_star, false);

    EXPECT_EQ(1U, policies[0]->parsing_errors.size());
    EXPECT_EQ(
        "The source list for Content-Security-Policy directive "
        "'frame-ancestors' contains a source with an invalid path: "
        "'/index.html?a=b'. The query component, including the '?', will be "
        "ignored.",
        policies[0]->parsing_errors[0]);
  }

  // Path containing ref.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader("Content-Security-Policy",
                       "frame-ancestors http://example.org/index.html#a;");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);
    EXPECT_EQ(1U, policies[0]->directives.size());

    auto& frame_ancestors =
        policies[0]->directives[mojom::CSPDirectiveName::FrameAncestors];
    EXPECT_EQ(frame_ancestors->sources.size(), 1U);
    EXPECT_EQ(frame_ancestors->sources[0]->scheme, "http");
    EXPECT_EQ(frame_ancestors->sources[0]->host, "example.org");
    EXPECT_EQ(frame_ancestors->sources[0]->port, url::PORT_UNSPECIFIED);
    EXPECT_EQ(frame_ancestors->sources[0]->path, "/index.html");
    EXPECT_EQ(frame_ancestors->sources[0]->is_host_wildcard, false);
    EXPECT_EQ(frame_ancestors->sources[0]->is_port_wildcard, false);
    EXPECT_EQ(frame_ancestors->allow_self, false);
    EXPECT_EQ(frame_ancestors->allow_star, false);

    EXPECT_EQ(1U, policies[0]->parsing_errors.size());
    EXPECT_EQ(
        "The source list for Content-Security-Policy directive "
        "'frame-ancestors' contains a source with an invalid path: "
        "'/index.html#a'. The fragment identifier, including the '#', will be "
        "ignored.",
        policies[0]->parsing_errors[0]);
  }

  // Multiple CSP headers with multiple frame-ancestors directives present.
  // Multiple policies should be created.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->AddHeader("Content-Security-Policy",
                       "frame-ancestors example.com");
    headers->AddHeader("Content-Security-Policy",
                       "frame-ancestors example.org");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);

    EXPECT_EQ(2U, policies.size());
    auto& frame_ancestors0 =
        policies[0]->directives[mojom::CSPDirectiveName::FrameAncestors];
    auto& frame_ancestors1 =
        policies[1]->directives[mojom::CSPDirectiveName::FrameAncestors];
    EXPECT_EQ(frame_ancestors0->sources.size(), 1U);
    EXPECT_EQ(frame_ancestors0->sources[0]->scheme, "");
    EXPECT_EQ(frame_ancestors0->sources[0]->host, "example.com");
    EXPECT_EQ(frame_ancestors0->sources[0]->port, url::PORT_UNSPECIFIED);
    EXPECT_EQ(frame_ancestors0->sources[0]->path, "");
    EXPECT_EQ(frame_ancestors0->sources[0]->is_host_wildcard, false);
    EXPECT_EQ(frame_ancestors0->sources[0]->is_port_wildcard, false);
    EXPECT_EQ(frame_ancestors0->allow_self, false);
    EXPECT_EQ(frame_ancestors0->allow_star, false);

    EXPECT_EQ(frame_ancestors1->sources.size(), 1U);
    EXPECT_EQ(frame_ancestors1->sources[0]->scheme, "");
    EXPECT_EQ(frame_ancestors1->sources[0]->host, "example.org");
    EXPECT_EQ(frame_ancestors1->sources[0]->port, url::PORT_UNSPECIFIED);
    EXPECT_EQ(frame_ancestors1->sources[0]->path, "");
    EXPECT_EQ(frame_ancestors1->sources[0]->is_host_wildcard, false);
    EXPECT_EQ(frame_ancestors1->sources[0]->is_port_wildcard, false);
    EXPECT_EQ(frame_ancestors1->allow_self, false);
    EXPECT_EQ(frame_ancestors1->allow_star, false);
  }

  // Multiple CSP headers separated by ',' (RFC2616 section 4.2).
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader("Content-Security-Policy",
                       "other-directive value, frame-ancestors example.org");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);

    EXPECT_EQ(2U, policies.size());
    auto& frame_ancestors1 =
        policies[1]->directives[mojom::CSPDirectiveName::FrameAncestors];
    EXPECT_EQ(frame_ancestors1->sources.size(), 1U);
    EXPECT_EQ(frame_ancestors1->sources[0]->scheme, "");
    EXPECT_EQ(frame_ancestors1->sources[0]->host, "example.org");
    EXPECT_EQ(frame_ancestors1->sources[0]->port, url::PORT_UNSPECIFIED);
    EXPECT_EQ(frame_ancestors1->sources[0]->path, "");
    EXPECT_EQ(frame_ancestors1->sources[0]->is_host_wildcard, false);
    EXPECT_EQ(frame_ancestors1->sources[0]->is_port_wildcard, false);
    EXPECT_EQ(frame_ancestors1->allow_self, false);
    EXPECT_EQ(frame_ancestors1->allow_star, false);
  }

  // Multiple CSP headers separated by ',', with multiple frame-ancestors
  // directives present. Multiple policies should be created.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader(
        "Content-Security-Policy",
        "frame-ancestors example.com, frame-ancestors example.org");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);

    EXPECT_EQ(2U, policies.size());
    auto& frame_ancestors0 =
        policies[0]->directives[mojom::CSPDirectiveName::FrameAncestors];
    auto& frame_ancestors1 =
        policies[1]->directives[mojom::CSPDirectiveName::FrameAncestors];
    EXPECT_EQ(frame_ancestors0->sources.size(), 1U);
    EXPECT_EQ(frame_ancestors0->sources[0]->scheme, "");
    EXPECT_EQ(frame_ancestors0->sources[0]->host, "example.com");
    EXPECT_EQ(frame_ancestors0->sources[0]->port, url::PORT_UNSPECIFIED);
    EXPECT_EQ(frame_ancestors0->sources[0]->path, "");
    EXPECT_EQ(frame_ancestors0->sources[0]->is_host_wildcard, false);
    EXPECT_EQ(frame_ancestors0->sources[0]->is_port_wildcard, false);
    EXPECT_EQ(frame_ancestors0->allow_self, false);
    EXPECT_EQ(frame_ancestors0->allow_star, false);

    EXPECT_EQ(frame_ancestors1->sources.size(), 1U);
    EXPECT_EQ(frame_ancestors1->sources[0]->scheme, "");
    EXPECT_EQ(frame_ancestors1->sources[0]->host, "example.org");
    EXPECT_EQ(frame_ancestors1->sources[0]->port, url::PORT_UNSPECIFIED);
    EXPECT_EQ(frame_ancestors1->sources[0]->path, "");
    EXPECT_EQ(frame_ancestors1->sources[0]->is_host_wildcard, false);
    EXPECT_EQ(frame_ancestors1->sources[0]->is_port_wildcard, false);
    EXPECT_EQ(frame_ancestors1->allow_self, false);
    EXPECT_EQ(frame_ancestors1->allow_star, false);
  }

  // Both frame-ancestors and report-to directives present.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader(
        "Content-Security-Policy",
        "report-to http://example.com/report; frame-ancestors example.com");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);

    auto& report_endpoints = policies[0]->report_endpoints;
    EXPECT_EQ(report_endpoints.size(), 1U);
    EXPECT_EQ(report_endpoints[0], "http://example.com/report");
    EXPECT_TRUE(policies[0]->use_reporting_api);

    auto& frame_ancestors =
        policies[0]->directives[mojom::CSPDirectiveName::FrameAncestors];
    EXPECT_EQ(frame_ancestors->sources.size(), 1U);
    EXPECT_EQ(frame_ancestors->sources[0]->scheme, "");
    EXPECT_EQ(frame_ancestors->sources[0]->host, "example.com");
    EXPECT_EQ(frame_ancestors->sources[0]->port, url::PORT_UNSPECIFIED);
    EXPECT_EQ(frame_ancestors->sources[0]->path, "");
    EXPECT_EQ(frame_ancestors->sources[0]->is_host_wildcard, false);
    EXPECT_EQ(frame_ancestors->sources[0]->is_port_wildcard, false);
    EXPECT_EQ(frame_ancestors->allow_self, false);
    EXPECT_EQ(frame_ancestors->allow_star, false);
  }
}

TEST(ContentSecurityPolicy, ParsePluginTypes) {
  {
    std::vector<mojom::ContentSecurityPolicyPtr> policies =
        ParseCSP("plugin-types    application/pdf text/plain  invalid a/a/a");
    EXPECT_EQ(policies[0]->directives.size(), 0u);
    EXPECT_TRUE(policies[0]->plugin_types.has_value());
    EXPECT_EQ(policies[0]->plugin_types.value().size(), 2u);
    EXPECT_EQ(policies[0]->plugin_types.value()[0], "application/pdf");
    EXPECT_EQ(policies[0]->plugin_types.value()[1], "text/plain");
    EXPECT_EQ(policies[0]->parsing_errors.size(), 2u);
    EXPECT_EQ(policies[0]->parsing_errors[0],
              "Invalid plugin type in 'plugin-types' Content Security Policy "
              "directive: 'invalid'.");
    EXPECT_EQ(policies[0]->parsing_errors[1],
              "Invalid plugin type in 'plugin-types' Content Security Policy "
              "directive: 'a/a/a'.");
  }

  {
    std::vector<mojom::ContentSecurityPolicyPtr> policies =
        ParseCSP("plugin-types ; default-src 'self'");
    EXPECT_TRUE(policies[0]->plugin_types.has_value());
    EXPECT_EQ(policies[0]->plugin_types.value().size(), 0u);
    EXPECT_EQ(policies[0]->parsing_errors.size(), 0u);
  }

  {
    std::vector<mojom::ContentSecurityPolicyPtr> policies =
        ParseCSP("plugin-types 'self' ; default-src 'self'");
    EXPECT_TRUE(policies[0]->plugin_types.has_value());
    EXPECT_EQ(policies[0]->plugin_types.value().size(), 0u);
    EXPECT_EQ(policies[0]->parsing_errors.size(), 1u);
    EXPECT_EQ(policies[0]->parsing_errors[0],
              "Invalid plugin type in 'plugin-types' Content Security Policy "
              "directive: ''self''.");
  }

  {
    std::vector<mojom::ContentSecurityPolicyPtr> policies =
        ParseCSP("default-src 'self'");
    EXPECT_FALSE(policies[0]->plugin_types.has_value());
    EXPECT_EQ(policies[0]->parsing_errors.size(), 0u);
  }
}

TEST(ContentSecurityPolicy, ParseReportEndpoint) {
  // report-uri directive.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader("Content-Security-Policy",
                       "report-uri http://example.com/report");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);

    auto& report_endpoints = policies[0]->report_endpoints;
    EXPECT_EQ(report_endpoints.size(), 1U);
    EXPECT_EQ(report_endpoints[0], "http://example.com/report");
    EXPECT_FALSE(policies[0]->use_reporting_api);
  }

  // report-to directive.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader("Content-Security-Policy",
                       "report-to http://example.com/report");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);

    auto& report_endpoints = policies[0]->report_endpoints;
    EXPECT_EQ(report_endpoints.size(), 1U);
    EXPECT_EQ(report_endpoints[0], "http://example.com/report");
    EXPECT_TRUE(policies[0]->use_reporting_api);
  }

  // Multiple directives. The report-to directive always takes priority.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader("Content-Security-Policy",
                       "report-uri http://example.com/report1; "
                       "report-uri http://example.com/report2; "
                       "report-to http://example.com/report3");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);

    auto& report_endpoints = policies[0]->report_endpoints;
    EXPECT_EQ(report_endpoints.size(), 1U);
    EXPECT_EQ(report_endpoints[0], "http://example.com/report3");
    EXPECT_TRUE(policies[0]->use_reporting_api);
  }
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->AddHeader("Content-Security-Policy",
                       "report-to http://example.com/report1");
    headers->AddHeader("Content-Security-Policy",
                       "report-uri http://example.com/report2");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);

    auto& report_endpoints = policies[0]->report_endpoints;
    EXPECT_EQ(report_endpoints.size(), 1U);
    EXPECT_EQ(report_endpoints[0], "http://example.com/report1");
    EXPECT_TRUE(policies[0]->use_reporting_api);
  }
}

// Check URL are upgraded iif "upgrade-insecure-requests" directive is defined.
TEST(ContentSecurityPolicy, ShouldUpgradeInsecureRequest) {
  std::vector<mojom::ContentSecurityPolicyPtr> policies;

  EXPECT_FALSE(ShouldUpgradeInsecureRequest(policies));

  policies.push_back(mojom::ContentSecurityPolicy::New());
  policies[0]->upgrade_insecure_requests = true;

  EXPECT_TRUE(ShouldUpgradeInsecureRequest(policies));
}

// Check upgraded URLs are only the Non-trusted Non-HTTP URLs.
TEST(ContentSecurityPolicy, UpgradeInsecureRequests) {
  std::vector<mojom::ContentSecurityPolicyPtr> policies;
  policies.push_back(mojom::ContentSecurityPolicy::New());
  policies[0]->upgrade_insecure_requests = true;

  struct {
    std::string input;
    std::string output;
  } kTestCases[]{
      // Non trusted Non-HTTP URLs are upgraded.
      {"http://example.com", "https://example.com"},
      {"http://example.com:80", "https://example.com:443"},

      // Non-standard ports should not be modified.
      {"http://example.com:8088", "https://example.com:8088"},

      // Trusted Non-HTTPS URLs don't need to be modified.
      {"http://127.0.0.1", "http://127.0.0.1"},
      {"http://127.0.0.8", "http://127.0.0.8"},
      {"http://localhost", "http://localhost"},
      {"http://sub.localhost", "http://sub.localhost"},

      // Non-HTTP URLs don't need to be modified.
      {"https://example.com", "https://example.com"},
      {"data:text/html,<html></html>", "data:text/html,<html></html>"},
      {"weird-scheme://this.is.a.url", "weird-scheme://this.is.a.url"},
  };

  for (const auto& test_case : kTestCases) {
    GURL url(test_case.input);
    UpgradeInsecureRequest(&url);
    EXPECT_EQ(url, GURL(test_case.output));
  }
}

TEST(ContentSecurityPolicy, NoDirective) {
  CSPContextTest context;

  EXPECT_TRUE(CheckContentSecurityPolicy(
      EmptyCSP(), CSPDirectiveName::FormAction, GURL("http://www.example.com"),
      false, false, &context, SourceLocation(), true));
  ASSERT_EQ(0u, context.violations().size());
}

TEST(ContentSecurityPolicy, ReportViolation) {
  CSPContextTest context;
  auto policy = BuildPolicy(CSPDirectiveName::FormAction,
                            BuildCSPSource("", "www.example.com"));

  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FormAction, GURL("http://www.not-example.com"),
      false, false, &context, SourceLocation(), true));

  ASSERT_EQ(1u, context.violations().size());
  const char console_message[] =
      "Refused to send form data to 'http://www.not-example.com/' because it "
      "violates the following Content Security Policy directive: \"form-action "
      "www.example.com\".\n";
  EXPECT_EQ(console_message, context.violations()[0]->console_message);
}

TEST(ContentSecurityPolicy, DirectiveFallback) {
  auto allow_host = [](const char* host) {
    std::vector<mojom::CSPSourcePtr> sources;
    sources.push_back(BuildCSPSource("http", host));
    auto csp_source_list = mojom::CSPSourceList::New();
    csp_source_list->sources = std::move(sources);
    return csp_source_list;
  };

  {
    CSPContextTest context;
    auto policy = EmptyCSP();
    policy->directives[CSPDirectiveName::DefaultSrc] = allow_host("a.com");
    EXPECT_FALSE(CheckContentSecurityPolicy(policy, CSPDirectiveName::FrameSrc,
                                            GURL("http://b.com"), false, false,
                                            &context, SourceLocation(), false));
    ASSERT_EQ(1u, context.violations().size());
    const char console_message[] =
        "Refused to frame 'http://b.com/' because it violates "
        "the following Content Security Policy directive: \"default-src "
        "http://a.com\". Note that 'frame-src' was not explicitly "
        "set, so 'default-src' is used as a fallback.\n";
    EXPECT_EQ(console_message, context.violations()[0]->console_message);
    EXPECT_TRUE(CheckContentSecurityPolicy(policy, CSPDirectiveName::FrameSrc,
                                           GURL("http://a.com"), false, false,
                                           &context, SourceLocation(), false));
  }
  {
    CSPContextTest context;
    auto policy = EmptyCSP();
    policy->directives[CSPDirectiveName::ChildSrc] = allow_host("a.com");
    EXPECT_FALSE(CheckContentSecurityPolicy(policy, CSPDirectiveName::FrameSrc,
                                            GURL("http://b.com"), false, false,
                                            &context, SourceLocation(), false));
    ASSERT_EQ(1u, context.violations().size());
    const char console_message[] =
        "Refused to frame 'http://b.com/' because it violates "
        "the following Content Security Policy directive: \"child-src "
        "http://a.com\". Note that 'frame-src' was not explicitly "
        "set, so 'child-src' is used as a fallback.\n";
    EXPECT_EQ(console_message, context.violations()[0]->console_message);
    EXPECT_TRUE(CheckContentSecurityPolicy(policy, CSPDirectiveName::FrameSrc,
                                           GURL("http://a.com"), false, false,
                                           &context, SourceLocation(), false));
  }
  {
    CSPContextTest context;
    auto policy = EmptyCSP();
    policy->directives[CSPDirectiveName::FrameSrc] = allow_host("a.com");
    policy->directives[CSPDirectiveName::ChildSrc] = allow_host("b.com");
    EXPECT_TRUE(CheckContentSecurityPolicy(policy, CSPDirectiveName::FrameSrc,
                                           GURL("http://a.com"), false, false,
                                           &context, SourceLocation(), false));
    EXPECT_FALSE(CheckContentSecurityPolicy(policy, CSPDirectiveName::FrameSrc,
                                            GURL("http://b.com"), false, false,
                                            &context, SourceLocation(), false));
    ASSERT_EQ(1u, context.violations().size());
    const char console_message[] =
        "Refused to frame 'http://b.com/' because it violates "
        "the following Content Security Policy directive: \"frame-src "
        "http://a.com\".\n";
    EXPECT_EQ(console_message, context.violations()[0]->console_message);
  }
}

TEST(ContentSecurityPolicy, RequestsAllowedWhenBypassingCSP) {
  CSPContextTest context;
  auto policy = DefaultSrc("https", "example.com");

  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("https://example.com/"), false,
      false, &context, SourceLocation(), false));
  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("https://not-example.com/"),
      false, false, &context, SourceLocation(), false));

  // Register 'https' as bypassing CSP, which should now bypass it entirely.
  context.AddSchemeToBypassCSP("https");

  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("https://example.com/"), false,
      false, &context, SourceLocation(), false));
  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("https://not-example.com/"),
      false, false, &context, SourceLocation(), false));
}

TEST(ContentSecurityPolicy, RequestsAllowedWhenHostMixedCase) {
  CSPContextTest context;
  auto policy = DefaultSrc("https", "ExAmPle.com");

  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("https://example.com/"), false,
      false, &context, SourceLocation(), false));
  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("https://not-example.com/"),
      false, false, &context, SourceLocation(), false));
}

TEST(ContentSecurityPolicy, FilesystemAllowedWhenBypassingCSP) {
  CSPContextTest context;
  auto policy = DefaultSrc("https", "example.com");

  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc,
      GURL("filesystem:https://example.com/file.txt"), false, false, &context,
      SourceLocation(), false));
  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc,
      GURL("filesystem:https://not-example.com/file.txt"), false, false,
      &context, SourceLocation(), false));

  // Register 'https' as bypassing CSP, which should now bypass it entirely.
  context.AddSchemeToBypassCSP("https");

  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc,
      GURL("filesystem:https://example.com/file.txt"), false, false, &context,
      SourceLocation(), false));
  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc,
      GURL("filesystem:https://not-example.com/file.txt"), false, false,
      &context, SourceLocation(), false));
}

TEST(ContentSecurityPolicy, BlobAllowedWhenBypassingCSP) {
  CSPContextTest context;
  auto policy = DefaultSrc("https", "example.com");

  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("blob:https://example.com/"),
      false, false, &context, SourceLocation(), false));
  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("blob:https://not-example.com/"),
      false, false, &context, SourceLocation(), false));

  // Register 'https' as bypassing CSP, which should now bypass it entirely.
  context.AddSchemeToBypassCSP("https");

  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("blob:https://example.com/"),
      false, false, &context, SourceLocation(), false));
  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("blob:https://not-example.com/"),
      false, false, &context, SourceLocation(), false));
}

TEST(ContentSecurityPolicy, NavigateToChecks) {
  GURL url_a("https://a");
  GURL url_b("https://b");
  CSPContextTest context;
  auto allow_none = [] { return mojom::CSPSourceList::New(); };
  auto allow_self = [] {
    auto csp = mojom::CSPSourceList::New();
    csp->allow_self = true;
    return csp;
  };
  auto allow_redirect = [] {
    auto csp = mojom::CSPSourceList::New();
    csp->allow_response_redirects = true;
    return csp;
  };
  auto source_a = [] {
    return mojom::CSPSource::New("https", "a", url::PORT_UNSPECIFIED, "", false,
                                 false);
  };
  auto allow_a = [&] {
    std::vector<mojom::CSPSourcePtr> sources;
    sources.push_back(source_a());
    auto csp = mojom::CSPSourceList::New();
    csp->sources = std::move(sources);
    return csp;
  };
  auto allow_redirect_a = [&] {
    std::vector<mojom::CSPSourcePtr> sources;
    sources.push_back(source_a());
    auto csp = mojom::CSPSourceList::New();
    csp->sources = std::move(sources);
    csp->allow_response_redirects = true;
    return csp;
  };
  context.SetSelf(source_a());

  struct TestCase {
    mojom::CSPSourceListPtr navigate_to_list;
    const GURL& url;
    bool is_response_check;
    bool is_form_submission;
    mojom::CSPSourceListPtr form_action_list;
    bool expected;
  } cases[] = {
      // Basic source matching.
      {allow_none(), url_a, false, false, {}, false},
      {allow_a(), url_a, false, false, {}, true},
      {allow_a(), url_b, false, false, {}, false},
      {allow_self(), url_a, false, false, {}, true},

      // Checking allow_redirect flag interactions.
      {allow_redirect(), url_a, false, false, {}, true},
      {allow_redirect(), url_a, true, false, {}, false},
      {allow_redirect_a(), url_a, false, false, {}, true},
      {allow_redirect_a(), url_a, true, false, {}, true},

      // Interaction with form-action:

      // Form submission without form-action present.
      {allow_none(), url_a, false, true, {}, false},
      {allow_a(), url_a, false, true, {}, true},
      {allow_a(), url_b, false, true, {}, false},
      {allow_self(), url_a, false, true, {}, true},

      // Form submission with form-action present.
      {allow_none(), url_a, false, true, allow_a(), true},
      {allow_a(), url_a, false, true, allow_a(), true},
      {allow_a(), url_b, false, true, allow_a(), true},
      {allow_self(), url_a, false, true, allow_a(), true},
  };

  for (auto& test : cases) {
    auto policy = EmptyCSP();
    policy->directives[CSPDirectiveName::NavigateTo] =
        std::move(test.navigate_to_list);

    if (test.form_action_list) {
      policy->directives[CSPDirectiveName::FormAction] =
          std::move(test.form_action_list);
    }

    EXPECT_EQ(test.expected, CheckContentSecurityPolicy(
                                 policy, CSPDirectiveName::NavigateTo, test.url,
                                 true, test.is_response_check, &context,
                                 SourceLocation(), test.is_form_submission));
    EXPECT_EQ(test.expected, CheckContentSecurityPolicy(
                                 policy, CSPDirectiveName::NavigateTo, test.url,
                                 false, test.is_response_check, &context,
                                 SourceLocation(), test.is_form_submission));
  }
}

TEST(ContentSecurityPolicy, ParseSandbox) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  headers->SetHeader("Content-Security-Policy",
                     "sandbox allow-downloads allow-scripts");
  std::vector<mojom::ContentSecurityPolicyPtr> policies;
  AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                      &policies);
  EXPECT_EQ(policies[0]->sandbox,
            mojom::WebSandboxFlags::kDownloads |
                mojom::WebSandboxFlags::kScripts |
                mojom::WebSandboxFlags::kAutomaticFeatures);
}

TEST(ContentSecurityPolicy, ParseSerializedSourceList) {
  struct TestCase {
    std::string directive_value;
    base::Callback<mojom::CSPSourceListPtr()> expected;
    std::string expected_error;
  } cases[] = {
      {
          "'nonce-a' 'nonce-a=' 'nonce-a==' 'nonce-a===' 'nonce-==' 'nonce-' "
          "'nonce 'nonce-cde' 'nonce-cde=' 'nonce-cde==' 'nonce-cde==='",
          base::Bind([] {
            auto csp = mojom::CSPSourceList::New();
            csp->nonces.push_back("a");
            csp->nonces.push_back("a=");
            csp->nonces.push_back("a==");
            csp->nonces.push_back("cde");
            csp->nonces.push_back("cde=");
            csp->nonces.push_back("cde==");
            return csp;
          }),
          "",
      },
      {
          "'sha256-abc' 'sha256-ABC' 'sha256 'sha256-' 'sha384-abc' "
          "'sha512-abc' 'sha-abc' 'sha256-*' 'sha-256-cde' 'sha-384-cde' "
          "'sha-512-cde'",
          base::Bind([] {
            auto csp = mojom::CSPSourceList::New();
            csp->hashes.push_back(mojom::CSPHashSource::New(
                mojom::CSPHashAlgorithm::SHA256, "abc"));
            csp->hashes.push_back(mojom::CSPHashSource::New(
                mojom::CSPHashAlgorithm::SHA256, "ABC"));
            csp->hashes.push_back(mojom::CSPHashSource::New(
                mojom::CSPHashAlgorithm::SHA384, "abc"));
            csp->hashes.push_back(mojom::CSPHashSource::New(
                mojom::CSPHashAlgorithm::SHA512, "abc"));
            csp->hashes.push_back(mojom::CSPHashSource::New(
                mojom::CSPHashAlgorithm::SHA256, "cde"));
            csp->hashes.push_back(mojom::CSPHashSource::New(
                mojom::CSPHashAlgorithm::SHA384, "cde"));
            csp->hashes.push_back(mojom::CSPHashSource::New(
                mojom::CSPHashAlgorithm::SHA512, "cde"));
            return csp;
          }),
          "",
      },
      {
          "'none' ",
          base::Bind([] { return mojom::CSPSourceList::New(); }),
          "",
      },
      {
          "'none' 'self'",
          base::Bind([] {
            auto csp = mojom::CSPSourceList::New();
            csp->allow_self = true;
            return csp;
          }),
          "The Content-Security-Policy directive 'script-src' contains the "
          "keyword 'none' alongside with other source expressions. The keyword "
          "'none' must be the only source expression in the directive value, "
          "otherwise it is ignored.",
      },
      {
          "'self' 'none'",
          base::Bind([] {
            auto csp = mojom::CSPSourceList::New();
            csp->allow_self = true;
            return csp;
          }),
          "The Content-Security-Policy directive 'script-src' contains the "
          "keyword 'none' alongside with other source expressions. The keyword "
          "'none' must be the only source expression in the directive value, "
          "otherwise it is ignored.",
      },
      {
          "'self'",
          base::Bind([] {
            auto csp = mojom::CSPSourceList::New();
            csp->allow_self = true;
            return csp;
          }),
      },
      {
          "'wrong' *",
          base::Bind([] {
            auto csp = mojom::CSPSourceList::New();
            csp->allow_star = true;
            return csp;
          }),
          "The source list for the Content-Security-Policy directive "
          "'script-src' contains an invalid source: ''wrong''.",
      },
      {
          "'wrong' 'unsafe-inline'",
          base::Bind([] {
            auto csp = mojom::CSPSourceList::New();
            csp->allow_inline = true;
            return csp;
          }),
          "The source list for the Content-Security-Policy directive "
          "'script-src' contains an invalid source: ''wrong''.",
      },
      {
          "'wrong' 'unsafe-eval'",
          base::Bind([] {
            auto csp = mojom::CSPSourceList::New();
            csp->allow_eval = true;
            return csp;
          }),
          "The source list for the Content-Security-Policy directive "
          "'script-src' contains an invalid source: ''wrong''.",
      },
      {
          "'wrong' 'wasm-eval'",
          base::Bind([] {
            auto csp = mojom::CSPSourceList::New();
            csp->allow_wasm_eval = true;
            return csp;
          }),
          "The source list for the Content-Security-Policy directive "
          "'script-src' contains an invalid source: ''wrong''.",
      },
      {
          "'wrong' 'strict-dynamic'",
          base::Bind([] {
            auto csp = mojom::CSPSourceList::New();
            csp->allow_dynamic = true;
            return csp;
          }),
          "The source list for the Content-Security-Policy directive "
          "'script-src' contains an invalid source: ''wrong''.",
      },
      {
          "'wrong' 'unsafe-hashes'",
          base::Bind([] {
            auto csp = mojom::CSPSourceList::New();
            csp->allow_unsafe_hashes = true;
            return csp;
          }),
          "The source list for the Content-Security-Policy directive "
          "'script-src' contains an invalid source: ''wrong''.",
      },
      {
          "'wrong' 'report-sample'",
          base::Bind([] {
            auto csp = mojom::CSPSourceList::New();
            csp->report_sample = true;
            return csp;
          }),
          "The source list for the Content-Security-Policy directive "
          "'script-src' contains an invalid source: ''wrong''.",
      },
  };

  for (auto& test : cases) {
    SCOPED_TRACE(test.directive_value);
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader("Content-Security-Policy",
                       "script-src " + test.directive_value);
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);
    EXPECT_TRUE(test.expected.Run().Equals(
        policies[0]->directives[mojom::CSPDirectiveName::ScriptSrc]));

    if (!test.expected_error.empty())
      EXPECT_EQ(test.expected_error, policies[0]->parsing_errors[0]);
  }
}

TEST(ContentSecurityPolicy, IsValidRequiredCSPAttr) {
  struct TestCase {
    const char* csp;
    bool expected;
    std::string expected_error;
  } cases[] = {{"script-src 'none'", true, ""},
               {"script-src 'none'; invalid-directive", false,
                "Parsing the csp attribute into a Content-Security-Policy "
                "returned one or more parsing errors: Unrecognized "
                "Content-Security-Policy directive 'invalid-directive'."},
               {"script-src 'none'; report-uri https://www.example.com", false,
                "The csp attribute cannot contain the directives 'report-to' "
                "or 'report-uri'."}};

  for (auto& test : cases) {
    SCOPED_TRACE(test.csp);
    std::vector<mojom::ContentSecurityPolicyPtr> csp;
    auto required_csp_headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    required_csp_headers->SetHeader("Content-Security-Policy", test.csp);
    AddContentSecurityPolicyFromHeaders(*required_csp_headers,
                                        GURL("https://example.com/"), &csp);
    std::string out;
    EXPECT_EQ(
        test.expected,
        IsValidRequiredCSPAttr(
            csp, nullptr, url::Origin::Create(GURL("https://a.com")), out));
    EXPECT_EQ(test.expected_error, out);
  }
}

TEST(ContentSecurityPolicy, Subsumes) {
  struct TestCase {
    std::string name;
    std::string required;
    std::string returned;
    bool returned_is_report_only;
    bool expected;
  } cases[] = {
      {
          "Required CSP but no returned CSP should return false.",
          "script-src 'none'",
          "",
          false,
          false,
      },
      {
          "Same CSP should return true.",
          "script-src 'none'",
          "script-src 'none'",
          false,
          true,
      },
      {
          "Same CSP returned in report-only mode should not be subsumed.",
          "script-src 'none'",
          "script-src 'none'",
          true,
          false,
      },
  };

  for (auto& test : cases) {
    std::vector<mojom::ContentSecurityPolicyPtr> required_csp =
        ParseCSP(test.required);

    auto returned_csp_headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    if (test.returned_is_report_only)
      returned_csp_headers->SetHeader("Content-Security-Policy-Report-Only",
                                      test.returned);
    else
      returned_csp_headers->SetHeader("Content-Security-Policy", test.returned);
    std::vector<mojom::ContentSecurityPolicyPtr> returned_csp;
    AddContentSecurityPolicyFromHeaders(
        *returned_csp_headers, GURL("https://example.com/"), &returned_csp);
    EXPECT_EQ(test.expected,
              Subsumes(*required_csp[0], returned_csp,
                       url::Origin::Create(GURL("https://a.com"))))
        << test.name;
  }
}

TEST(ContentSecurityPolicy, SubsumesBasedOnCSPSourcesOnly) {
  const char* csp_a =
      "script-src http://*.one.com; img-src https://sub.one.com "
      "http://two.com/imgs/";

  struct TestCase {
    const char* policies;
    bool expected;
    bool expected_first_policy_opposite;
  } cases[] = {
      // `listB`, which is not as restrictive as `A`, is not subsumed.
      {"", false, true},
      {"script-src http://example.com", false, false},
      {"img-src http://example.com", false, false},
      {"script-src http://*.one.com", false, true},
      {"img-src https://sub.one.com http://two.com/imgs/", false, true},
      {"default-src http://example.com", false, false},
      {"default-src https://sub.one.com http://two.com/imgs/", false, false},
      {"default-src http://sub.one.com", false, false},
      {"script-src http://*.one.com; img-src http://two.com/", false, false},
      {"script-src http://*.one.com, img-src http://sub.one.com", false, true},
      {"script-src http://*.one.com, script-src https://two.com", false, true},
      {"script-src http://*.random.com,"
       "script-src https://random.com",
       false, false},
      {"script-src http://sub.one.com,"
       "script-src https://random.com",
       false, false},
      {"script-src http://*.random.com; default-src http://sub.one.com "
       "http://two.com/imgs/,"
       "default-src https://sub.random.com",
       false, false},
      // `listB`, which is as restrictive as `A`, is subsumed.
      {"default-src https://sub.one.com", true, false},
      {"default-src http://random.com,"
       "default-src https://non-random.com:*",
       true, false},
      {"script-src http://*.one.com; img-src https://sub.one.com", true, false},
      {"script-src http://*.one.com; img-src https://sub.one.com "
       "http://two.com/imgs/",
       true, true},
      {"script-src http://*.one.com,"
       "img-src https://sub.one.com http://two.com/imgs/",
       true, true},
      {"script-src http://*.random.com; default-src https://sub.one.com "
       "http://two.com/imgs/,"
       "default-src https://else.com",
       true, false},
      {"script-src http://*.random.com; default-src https://sub.one.com "
       "http://two.com/imgs/,"
       "default-src https://sub.one.com",
       true, false},
  };

  std::vector<mojom::ContentSecurityPolicyPtr> policy_a = ParseCSP(csp_a);

  for (const auto& test : cases) {
    std::vector<mojom::ContentSecurityPolicyPtr> policies_b =
        ParseCSP(test.policies);
    EXPECT_EQ(Subsumes(*policy_a[0], policies_b,
                       url::Origin::Create(GURL("https://a.com"))),
              test.expected)
        << csp_a << " should " << (test.expected ? "" : "not ") << "subsume "
        << test.policies;

    if (!policies_b.empty()) {
      // Check if first policy of `listB` subsumes `A`.
      EXPECT_EQ(Subsumes(*policies_b[0], policy_a,
                         url::Origin::Create(GURL("https://a.com"))),
                test.expected_first_policy_opposite)
          << csp_a << " should "
          << (test.expected_first_policy_opposite ? "" : "not ") << "subsume "
          << test.policies;
    }
  }
}

TEST(ContentSecurityPolicy, SubsumesIfNoneIsPresent) {
  struct TestCase {
    const char* policy_a;
    const char* policies_b;
    bool expected;
  } cases[] = {
      // `policyA` is 'none', but no policy in `policiesB` is.
      {"script-src ", "", false},
      {"script-src 'none'", "", false},
      {"script-src ", "script-src http://example.com", false},
      {"script-src 'none'", "script-src http://example.com", false},
      {"script-src ", "img-src 'none'", false},
      {"script-src 'none'", "img-src 'none'", false},
      {"script-src ", "script-src http://*.one.com, img-src https://two.com",
       false},
      {"script-src 'none'",
       "script-src http://*.one.com, img-src https://two.com", false},
      {"script-src 'none'",
       "script-src http://*.one.com, script-src https://two.com", true},
      {"script-src 'none'", "script-src http://*.one.com, script-src 'self'",
       true},
      // `policyA` is not 'none', but at least effective result of `policiesB`
      // is.
      {"script-src http://example.com 'none'", "script-src 'none'", true},
      {"script-src http://example.com", "script-src 'none'", true},
      {"script-src http://example.com 'none'",
       "script-src http://*.one.com, script-src http://sub.one.com,"
       "script-src 'none'",
       true},
      {"script-src http://example.com",
       "script-src http://*.one.com, script-src http://sub.one.com,"
       "script-src 'none'",
       true},
      {"script-src http://one.com 'none'",
       "script-src http://*.one.com, script-src http://sub.one.com,"
       "script-src https://one.com",
       true},
      // `policyA` is `none` and at least effective result of `policiesB` is
      // too.
      {"script-src ", "script-src , script-src ", true},
      {"script-src 'none'", "script-src, script-src 'none'", true},
      {"script-src ", "script-src 'none', script-src 'none'", true},
      {"script-src ",
       "script-src 'none' http://example.com,"
       "script-src 'none' http://example.com",
       false},
      {"script-src 'none'", "script-src 'none', script-src 'none'", true},
      {"script-src 'none'",
       "script-src 'none', script-src 'none', script-src 'none'", true},
      {"script-src 'none'",
       "script-src http://*.one.com, script-src http://sub.one.com,"
       "script-src 'none'",
       true},
      {"script-src 'none'",
       "script-src http://*.one.com, script-src http://two.com,"
       "script-src http://three.com",
       true},
      // Policies contain special keywords.
      {"script-src ", "script-src , script-src 'unsafe-eval'", true},
      {"script-src 'none'", "script-src 'unsafe-inline', script-src 'none'",
       true},
      {"script-src ",
       "script-src 'none' 'unsafe-inline',"
       "script-src 'none' 'unsafe-inline'",
       false},
      {"script-src ",
       "script-src 'none' 'unsafe-inline',"
       "script-src 'unsafe-inline' 'strict-dynamic'",
       false},
      {"script-src 'unsafe-eval'",
       "script-src 'unsafe-eval', script 'unsafe-inline'", true},
      {"script-src 'unsafe-inline'", "script-src  , script http://example.com",
       true},
  };

  for (const auto& test : cases) {
    std::vector<mojom::ContentSecurityPolicyPtr> policy_a =
        ParseCSP(test.policy_a);
    std::vector<mojom::ContentSecurityPolicyPtr> policies_b =
        ParseCSP(test.policies_b);
    EXPECT_EQ(Subsumes(*policy_a[0], policies_b,
                       url::Origin::Create(GURL("https://a.com"))),
              test.expected)
        << test.policy_a << " should " << (test.expected ? "" : "not ")
        << "subsume " << test.policies_b;
  }
}

TEST(ContentSecurityPolicy, SubsumesPluginTypes) {
  struct TestCase {
    const char* policy_a;
    const char* policies_b;
    bool expected;
  } cases[] = {
      // `policyA` subsumes `policiesB`.
      {"script-src 'unsafe-inline'",
       "script-src  , script-src http://example.com, plugin-types text/plain",
       true},
      {"script-src http://example.com",
       "script-src http://example.com; plugin-types ", true},
      {"script-src http://example.com",
       "script-src http://example.com; plugin-types text/plain", true},
      {"script-src http://example.com; plugin-types text/plain",
       "script-src http://example.com; plugin-types text/plain", true},
      {"script-src http://example.com; plugin-types text/plain",
       "script-src http://example.com; plugin-types ", true},
      {"script-src http://example.com; plugin-types text/plain",
       "script-src http://example.com; plugin-types , plugin-types ", true},
      {"plugin-types application/pdf text/plain",
       "plugin-types application/pdf text/plain, plugin-types "
       "application/x-blink-test-plugin",
       true},
      {"plugin-types application/pdf text/plain",
       "plugin-types application/pdf text/plain,"
       "plugin-types application/pdf text/plain "
       "application/x-blink-test-plugin",
       true},
      {"plugin-types application/x-shockwave-flash application/pdf text/plain",
       "plugin-types application/x-shockwave-flash application/pdf text/plain, "
       "plugin-types application/x-shockwave-flash",
       true},
      {"plugin-types application/x-shockwave-flash",
       "plugin-types application/x-shockwave-flash application/pdf text/plain, "
       "plugin-types application/x-shockwave-flash",
       true},
      // `policyA` does not subsume `policiesB`.
      {"script-src http://example.com; plugin-types text/plain", "", false},
      {"script-src http://example.com; plugin-types text/plain",
       "script-src http://example.com", false},
      {"plugin-types random-value",
       "script-src 'unsafe-inline', plugin-types text/plain", false},
      {"plugin-types random-value",
       "script-src http://example.com, script-src http://example.com", false},
      {"plugin-types random-value",
       "plugin-types  text/plain, plugin-types text/plain", false},
      {"script-src http://example.com; plugin-types text/plain",
       "plugin-types , plugin-types ", false},
      {"plugin-types application/pdf text/plain",
       "plugin-types application/x-blink-test-plugin,"
       "plugin-types application/x-blink-test-plugin",
       false},
      {"plugin-types application/pdf text/plain",
       "plugin-types application/pdf application/x-blink-test-plugin, "
       "plugin-types application/x-blink-test-plugin",
       false},
  };

  for (const auto& test : cases) {
    std::vector<mojom::ContentSecurityPolicyPtr> policy_a =
        ParseCSP(test.policy_a);
    std::vector<mojom::ContentSecurityPolicyPtr> policies_b =
        ParseCSP(test.policies_b);
    EXPECT_EQ(Subsumes(*policy_a[0], policies_b,
                       url::Origin::Create(GURL("https://a.com"))),
              test.expected)
        << test.policy_a << " should " << (test.expected ? "" : "not ")
        << "subsume " << test.policies_b;
  }
}

}  // namespace network
