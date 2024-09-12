// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_security_policy/content_security_policy.h"

#include "base/containers/contains.h"
#include "base/memory/raw_ref.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/content_security_policy/csp_context.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
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
  EXPECT_EQ(
      policies[0]->raw_directives[mojom::CSPDirectiveName::FrameAncestors],
      header);
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

  CSPContextTest(const CSPContextTest&) = delete;
  CSPContextTest& operator=(const CSPContextTest&) = delete;

  const std::vector<network::mojom::CSPViolationPtr>& violations() {
    return violations_;
  }

  void AddSchemeToBypassCSP(const std::string& scheme) {
    scheme_to_bypass_.push_back(scheme);
  }

  bool SchemeShouldBypassCSP(std::string_view scheme) override {
    return base::Contains(scheme_to_bypass_, scheme);
  }

 private:
  void ReportContentSecurityPolicyViolation(
      network::mojom::CSPViolationPtr violation) override {
    violations_.push_back(std::move(violation));
  }
  std::vector<network::mojom::CSPViolationPtr> violations_;
  std::vector<std::string> scheme_to_bypass_;
};

mojom::ContentSecurityPolicyPtr EmptyCSP() {
  auto policy = mojom::ContentSecurityPolicy::New();
  policy->header = mojom::ContentSecurityPolicyHeader::New();
  policy->self_origin = network::mojom::CSPSource::New(
      "", "", url::PORT_UNSPECIFIED, "", false, false);
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

mojom::ContentSecurityPolicyPtr ParseOneCspReportOnly(std::string expression) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  headers->SetHeader("Content-Security-Policy-Report-Only", expression);
  std::vector<mojom::ContentSecurityPolicyPtr> policies;
  AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                      &policies);
  CHECK_EQ(1u, policies.size());
  return std::move(policies[0]);
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
      {".b.c"},
      {"a..c"},

      // Trailing dots
      {"a.", {{{"", "a."}}}},
      {"a.b.", {{{"", "a.b."}}}},

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
  // Directive names are case-insensitive.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader("Content-Security-Policy",
                       "ScrIPT-sRc 'none'; img-src 'none'");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);
    EXPECT_EQ(2U, policies[0]->directives.size());
    EXPECT_EQ(2U, policies[0]->raw_directives.size());

    EXPECT_EQ(policies[0]->raw_directives[mojom::CSPDirectiveName::ScriptSrc],
              "'none'");
    auto& script_src =
        policies[0]->directives[mojom::CSPDirectiveName::ScriptSrc];
    EXPECT_EQ(script_src->sources.size(), 0U);
    EXPECT_EQ(script_src->allow_self, false);
    EXPECT_EQ(script_src->allow_star, false);

    EXPECT_EQ(policies[0]->raw_directives[mojom::CSPDirectiveName::ImgSrc],
              "'none'");
    auto& img_src = policies[0]->directives[mojom::CSPDirectiveName::ScriptSrc];
    EXPECT_EQ(img_src->sources.size(), 0U);
    EXPECT_EQ(img_src->allow_self, false);
    EXPECT_EQ(img_src->allow_star, false);

    EXPECT_THAT(policies[0]->parsing_errors, testing::IsEmpty());
  }

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
    EXPECT_EQ(2U, policies[0]->raw_directives.size());

    EXPECT_EQ(
        policies[0]->raw_directives[mojom::CSPDirectiveName::FrameAncestors],
        "example.com");
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

    EXPECT_EQ(policies[0]->raw_directives[mojom::CSPDirectiveName::ScriptSrc],
              "example2.com");
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
    EXPECT_EQ(1U, policies[0]->raw_directives.size());

    EXPECT_EQ(
        policies[0]->raw_directives[mojom::CSPDirectiveName::FrameAncestors],
        "example.org");
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
    EXPECT_TRUE(policies[0]->raw_directives.empty());

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
        "contains one or more invalid characters. In a source expression, "
        "non-whitespace characters outside ASCII 0x21-0x7E must be "
        "Punycode-encoded, as described in RFC 3492 "
        "(https://tools.ietf.org/html/rfc3492), if part of the hostname and "
        "percent-encoded, as described in RFC 3986, section 2.1 "
        "(http://tools.ietf.org/html/rfc3986#section-2.1), if part of the "
        "path.",
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
    EXPECT_EQ(1U, policies[0]->raw_directives.size());

    EXPECT_EQ(
        policies[0]->raw_directives[mojom::CSPDirectiveName::FrameAncestors],
        "object-src");
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
    EXPECT_EQ(1U, policies[0]->raw_directives.size());

    EXPECT_EQ(
        policies[0]->raw_directives[mojom::CSPDirectiveName::FrameAncestors],
        "http://example.org/index.html?a=b");
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
        "The source list for Content Security Policy directive "
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
    EXPECT_EQ(1U, policies[0]->raw_directives.size());

    EXPECT_EQ(
        policies[0]->raw_directives[mojom::CSPDirectiveName::FrameAncestors],
        "http://example.org/index.html#a");
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
        "The source list for Content Security Policy directive "
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

    EXPECT_EQ(
        policies[0]->raw_directives[mojom::CSPDirectiveName::FrameAncestors],
        "example.com");
    EXPECT_EQ(
        policies[1]->raw_directives[mojom::CSPDirectiveName::FrameAncestors],
        "example.org");

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
    EXPECT_EQ(
        policies[1]->raw_directives[mojom::CSPDirectiveName::FrameAncestors],
        "example.org");

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

    EXPECT_EQ(
        policies[0]->raw_directives[mojom::CSPDirectiveName::FrameAncestors],
        "example.com");
    EXPECT_EQ(
        policies[1]->raw_directives[mojom::CSPDirectiveName::FrameAncestors],
        "example.org");

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

    EXPECT_EQ(
        policies[0]->raw_directives[mojom::CSPDirectiveName::FrameAncestors],
        "example.com");

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

TEST(ContentSecurityPolicy, ParseAllow) {
  std::vector<mojom::ContentSecurityPolicyPtr> policies =
      ParseCSP("allow    'none'");
  EXPECT_EQ(policies[0]->directives.size(), 0u);
  EXPECT_EQ(
      policies[0]->parsing_errors[0],
      "The 'allow' directive has been replaced with 'default-src'. Please "
      "use that directive instead, as 'allow' has no effect.");
}

TEST(ContentSecurityPolicy, ParseOptions) {
  std::vector<mojom::ContentSecurityPolicyPtr> policies =
      ParseCSP("options 'unsafe-inline'");
  EXPECT_EQ(policies[0]->directives.size(), 0u);
  EXPECT_EQ(policies[0]->parsing_errors[0],
            "The 'options' directive has been replaced with the "
            "'unsafe-inline' and 'unsafe-eval' source expressions for the "
            "'script-src' and 'style-src' directives. Please use those "
            "directives instead, as 'options' has no effect.");
}

TEST(ContentSecurityPolicy, ParsePolicyUri) {
  std::vector<mojom::ContentSecurityPolicyPtr> policies =
      ParseCSP("policy-uri   https://example.com/my-csp");
  EXPECT_EQ(policies[0]->directives.size(), 0u);
  EXPECT_EQ(policies[0]->parsing_errors[0],
            "The 'policy-uri' directive has been removed from the "
            "specification. Please specify a complete policy via the "
            "Content-Security-Policy header.");
}

TEST(ContentSecurityPolicy, ParsePluginTypes) {
  std::vector<mojom::ContentSecurityPolicyPtr> policies =
      ParseCSP("plugin-types    application/pdf text/plain");
  EXPECT_EQ(policies[0]->directives.size(), 0u);
  EXPECT_EQ(policies[0]->parsing_errors[0],
            "The Content-Security-Policy directive 'plugin-types' has been "
            "removed from the "
            "specification. If you want to block plugins, consider specifying "
            "\"object-src 'none'\" instead.");
}

TEST(ContentSecurityPolicy, ParseRequireTrustedTypesFor) {
  struct {
    const char* input;
    const unsigned long errors;
    network::mojom::CSPRequireTrustedTypesFor expected;
  } cases[]{
      {
          "",
          1u,
          network::mojom::CSPRequireTrustedTypesFor::None,
      },
      {
          "'script'",
          0u,
          network::mojom::CSPRequireTrustedTypesFor::Script,
      },
      {
          "'wasm' 'script'",
          1u,
          network::mojom::CSPRequireTrustedTypesFor::Script,
      },
      {
          "'script' 'wasm' 'script'",
          1u,
          network::mojom::CSPRequireTrustedTypesFor::Script,
      },
      {
          "'wasm'",
          2u,
          network::mojom::CSPRequireTrustedTypesFor::None,
      },
  };

  for (const auto& testCase : cases) {
    std::vector<mojom::ContentSecurityPolicyPtr> policies = ParseCSP(
        base::StringPrintf("require-trusted-types-for %s", testCase.input));
    EXPECT_EQ(
        policies[0]
            ->raw_directives[mojom::CSPDirectiveName::RequireTrustedTypesFor],
        testCase.input);
    EXPECT_EQ(policies[0]->directives.size(), 0u);
    EXPECT_EQ(policies[0]->parsing_errors.size(), testCase.errors);
    EXPECT_EQ(policies[0]->require_trusted_types_for, testCase.expected);
  }
}

TEST(ContentSecurityPolicy, ParseTrustedTypes) {
  {
    std::vector<mojom::ContentSecurityPolicyPtr> policies =
        ParseCSP("script-src 'none'");
    EXPECT_EQ(policies[0]->directives.size(), 1u);
    EXPECT_FALSE(policies[0]->trusted_types);
  }

  {
    std::vector<mojom::ContentSecurityPolicyPtr> policies =
        ParseCSP("trusted-types 'none'");
    EXPECT_EQ(
        policies[0]->raw_directives[mojom::CSPDirectiveName::TrustedTypes],
        "'none'");
    EXPECT_EQ(policies[0]->directives.size(), 0u);
    EXPECT_TRUE(policies[0]->trusted_types);
    EXPECT_EQ(policies[0]->trusted_types->list.size(), 0u);
    EXPECT_FALSE(policies[0]->trusted_types->allow_any);
    EXPECT_FALSE(policies[0]->trusted_types->allow_duplicates);
    EXPECT_EQ(policies[0]->trusted_types->list.size(), 0u);
    EXPECT_EQ(policies[0]->parsing_errors.size(), 0u);
  }

  {
    std::vector<mojom::ContentSecurityPolicyPtr> policies =
        ParseCSP("trusted-types policy   'none'  other_policy@ invalid~policy");
    EXPECT_EQ(
        policies[0]->raw_directives[mojom::CSPDirectiveName::TrustedTypes],
        "policy   'none'  other_policy@ invalid~policy");
    EXPECT_EQ(policies[0]->directives.size(), 0u);
    EXPECT_TRUE(policies[0]->trusted_types);
    EXPECT_EQ(policies[0]->trusted_types->list.size(), 2u);
    EXPECT_EQ(policies[0]->trusted_types->list[0], "policy");
    EXPECT_EQ(policies[0]->trusted_types->list[1], "other_policy@");
    EXPECT_FALSE(policies[0]->trusted_types->allow_any);
    EXPECT_FALSE(policies[0]->trusted_types->allow_duplicates);
    EXPECT_EQ(policies[0]->parsing_errors.size(), 2u);
    EXPECT_EQ(
        policies[0]->parsing_errors[0],
        "The value of the Content Security Policy directive 'trusted_types' "
        "contains an invalid policy: 'none'. It will be ignored. "
        "Note that 'none' has no effect unless it is the only "
        "expression in the directive value.");
    EXPECT_EQ(
        policies[0]->parsing_errors[1],
        "The value of the Content Security Policy directive 'trusted_types' "
        "contains an invalid policy: 'invalid~policy'. It will be ignored.");
  }
}

TEST(ContentSecurityPolicy, ParseBlockAllMixedContent) {
  {
    std::vector<mojom::ContentSecurityPolicyPtr> policies =
        ParseCSP("script-src 'none'");
    EXPECT_EQ(policies[0]->directives.size(), 1u);
    EXPECT_FALSE(policies[0]->block_all_mixed_content);
  }

  {
    std::vector<mojom::ContentSecurityPolicyPtr> policies =
        ParseCSP("block-all-mixed-content");
    EXPECT_EQ(policies[0]->directives.size(), 0u);
    EXPECT_TRUE(policies[0]->block_all_mixed_content);
    EXPECT_EQ(policies[0]->parsing_errors.size(), 0u);
  }

  {
    std::vector<mojom::ContentSecurityPolicyPtr> policies =
        ParseCSP("block-all-mixed-content true");
    EXPECT_EQ(policies[0]->directives.size(), 0u);
    EXPECT_TRUE(policies[0]->block_all_mixed_content);
    EXPECT_EQ(policies[0]->parsing_errors.size(), 1u);
    EXPECT_EQ(policies[0]->parsing_errors[0],
              "The Content Security Policy directive "
              "'block-all-mixed-content' should be empty, but was delivered "
              "with a value of 'true'. The directive has been applied, and the "
              "value ignored.");
  }
}

TEST(ContentSecurityPolicy, ParseReportEndpoint) {
  // report-uri directive.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader("Content-Security-Policy", "report-uri report");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("http://example.com/"),
                                        &policies);

    auto& report_endpoints = policies[0]->report_endpoints;
    EXPECT_EQ(report_endpoints.size(), 1U);
    EXPECT_EQ(report_endpoints[0], "http://example.com/report");
    EXPECT_FALSE(policies[0]->use_reporting_api);
  }

  // report-uri directive, url ignored because of mixed content.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader("Content-Security-Policy",
                       "report-uri http://example.com/report");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);

    auto& report_endpoints = policies[0]->report_endpoints;
    EXPECT_TRUE(report_endpoints.empty());
    EXPECT_EQ(policies[0]->parsing_errors.size(), 1U);
    EXPECT_EQ(policies[0]->parsing_errors[0],
              "The Content Security Policy directive 'report-uri' specifies as "
              "endpoint 'http://example.com/report'. This endpoint will be "
              "ignored since it violates the policy for Mixed Content.");
  }

  // report-to directive.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader("Content-Security-Policy", "report-to group");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);

    auto& report_endpoints = policies[0]->report_endpoints;
    EXPECT_EQ(report_endpoints.size(), 1U);
    EXPECT_EQ(report_endpoints[0], "group");
    EXPECT_TRUE(policies[0]->use_reporting_api);
  }

  // Multiple report-to directive.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader("Content-Security-Policy",
                       "report-to group1 group2 group3");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);

    auto& report_endpoints = policies[0]->report_endpoints;
    EXPECT_EQ(report_endpoints.size(), 1U);
    EXPECT_EQ(report_endpoints[0], "group1");
    EXPECT_TRUE(policies[0]->use_reporting_api);
    EXPECT_EQ(policies[0]->parsing_errors.size(), 1U);
    EXPECT_EQ(policies[0]->parsing_errors[0],
              "The Content Security Policy directive 'report-to' contains more "
              "than one endpoint. Only the first one will be used, the other "
              "ones will be ignored.");
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
    AddContentSecurityPolicyFromHeaders(*headers, GURL("http://example.com/"),
                                        &policies);

    auto& report_endpoints = policies[0]->report_endpoints;
    EXPECT_EQ(report_endpoints.size(), 1U);
    EXPECT_EQ(report_endpoints[0], "http://example.com/report1");
    EXPECT_TRUE(policies[0]->use_reporting_api);
  }
}

TEST(ContentSecurityPolicy, ParseStoresSelfOrigin) {
  struct {
    const char* url;
    network::mojom::CSPSourcePtr self_origin;
  } testCases[]{
      {
          "https://example.com",
          network::mojom::CSPSource::New("https", "example.com", 443, "", false,
                                         false),
      },
      {
          "http://example.com/main/index.html",
          network::mojom::CSPSource::New("http", "example.com", 80, "", false,
                                         false),
      },
      {
          "file://localhost/var/www/index.html",
          network::mojom::CSPSource::New("file", "", url::PORT_UNSPECIFIED, "",
                                         false, false),
      },
  };

  for (const auto& testCase : testCases) {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader("Content-Security-Policy", "default-src 'none'");
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL(testCase.url),
                                        &policies);

    EXPECT_TRUE(testCase.self_origin.Equals(policies[0]->self_origin));
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
      GURL(), false, &context, SourceLocation(), true));
  ASSERT_EQ(0u, context.violations().size());
}

TEST(ContentSecurityPolicy, ReportViolation) {
  CSPContextTest context;
  auto policy = BuildPolicy(CSPDirectiveName::FormAction,
                            BuildCSPSource("", "www.example.com"));

  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FormAction, GURL("http://www.not-example.com"),
      GURL("http://www.example.com"), false, &context, SourceLocation(), true));

  ASSERT_EQ(1u, context.violations().size());
  const char console_message[] =
      "Refused to send form data to 'http://www.example.com/' because it "
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
                                            GURL("http://b.com"), GURL(), false,
                                            &context, SourceLocation(), false));
    ASSERT_EQ(1u, context.violations().size());
    const char console_message[] =
        "Refused to frame 'http://b.com/' because it violates "
        "the following Content Security Policy directive: \"default-src "
        "http://a.com\". Note that 'frame-src' was not explicitly "
        "set, so 'default-src' is used as a fallback.\n";
    EXPECT_EQ(console_message, context.violations()[0]->console_message);
    EXPECT_TRUE(CheckContentSecurityPolicy(policy, CSPDirectiveName::FrameSrc,
                                           GURL("http://a.com"), GURL(), false,
                                           &context, SourceLocation(), false));
  }
  {
    CSPContextTest context;
    auto policy = EmptyCSP();
    policy->directives[CSPDirectiveName::ChildSrc] = allow_host("a.com");
    EXPECT_FALSE(CheckContentSecurityPolicy(policy, CSPDirectiveName::FrameSrc,
                                            GURL("http://b.com"), GURL(), false,
                                            &context, SourceLocation(), false));
    ASSERT_EQ(1u, context.violations().size());
    const char console_message[] =
        "Refused to frame 'http://b.com/' because it violates "
        "the following Content Security Policy directive: \"child-src "
        "http://a.com\". Note that 'frame-src' was not explicitly "
        "set, so 'child-src' is used as a fallback.\n";
    EXPECT_EQ(console_message, context.violations()[0]->console_message);
    EXPECT_TRUE(CheckContentSecurityPolicy(policy, CSPDirectiveName::FrameSrc,
                                           GURL("http://a.com"), GURL(), false,
                                           &context, SourceLocation(), false));
  }
  {
    CSPContextTest context;
    auto policy = EmptyCSP();
    policy->directives[CSPDirectiveName::FrameSrc] = allow_host("a.com");
    policy->directives[CSPDirectiveName::ChildSrc] = allow_host("b.com");
    EXPECT_TRUE(CheckContentSecurityPolicy(policy, CSPDirectiveName::FrameSrc,
                                           GURL("http://a.com"), GURL(), false,
                                           &context, SourceLocation(), false));
    EXPECT_FALSE(CheckContentSecurityPolicy(policy, CSPDirectiveName::FrameSrc,
                                            GURL("http://b.com"), GURL(), false,
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
      policy, CSPDirectiveName::FrameSrc, GURL("https://example.com/"), GURL(),
      false, &context, SourceLocation(), false));
  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("https://not-example.com/"),
      GURL(), false, &context, SourceLocation(), false));

  // Register 'https' as bypassing CSP, which should now bypass it entirely.
  context.AddSchemeToBypassCSP("https");

  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("https://example.com/"), GURL(),
      false, &context, SourceLocation(), false));
  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("https://not-example.com/"),
      GURL(), false, &context, SourceLocation(), false));
}

TEST(ContentSecurityPolicy, FilesystemAllowedWhenBypassingCSP) {
  CSPContextTest context;
  auto policy = DefaultSrc("https", "example.com");

  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc,
      GURL("filesystem:https://example.com/file.txt"), GURL(), false, &context,
      SourceLocation(), false));
  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc,
      GURL("filesystem:https://not-example.com/file.txt"), GURL(), false,
      &context, SourceLocation(), false));

  // Register 'https' as bypassing CSP, which should now bypass it entirely.
  context.AddSchemeToBypassCSP("https");

  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc,
      GURL("filesystem:https://example.com/file.txt"), GURL(), false, &context,
      SourceLocation(), false));
  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc,
      GURL("filesystem:https://not-example.com/file.txt"), GURL(), false,
      &context, SourceLocation(), false));
}

TEST(ContentSecurityPolicy, BlobAllowedWhenBypassingCSP) {
  CSPContextTest context;
  auto policy = DefaultSrc("https", "example.com");

  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("blob:https://example.com/"),
      GURL(), false, &context, SourceLocation(), false));
  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("blob:https://not-example.com/"),
      GURL(), false, &context, SourceLocation(), false));

  // Register 'https' as bypassing CSP, which should now bypass it entirely.
  context.AddSchemeToBypassCSP("https");

  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("blob:https://example.com/"),
      GURL(), false, &context, SourceLocation(), false));
  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("blob:https://not-example.com/"),
      GURL(), false, &context, SourceLocation(), false));
}

TEST(ContentSecurityPolicy, ParseSandbox) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  headers->SetHeader("Content-Security-Policy",
                     "sandbox allow-downloads allow-scripts");
  std::vector<mojom::ContentSecurityPolicyPtr> policies;
  AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                      &policies);
  EXPECT_EQ(policies[0]->raw_directives[mojom::CSPDirectiveName::Sandbox],
            "allow-downloads allow-scripts");
  EXPECT_EQ(policies[0]->sandbox,
            ~mojom::WebSandboxFlags::kDownloads &
                ~mojom::WebSandboxFlags::kScripts &
                ~mojom::WebSandboxFlags::kAutomaticFeatures);
}

TEST(ContentSecurityPolicy, ParseSerializedSourceList) {
  struct TestCase {
    std::string directive_value;
    base::OnceCallback<mojom::CSPSourceListPtr()> expected;
    std::string expected_error;
  } cases[] = {
      {
          "'nonce-a' 'nonce-a=' 'nonce-a==' 'nonce-a===' 'nonce-==' 'nonce-' "
          "'nonce 'nonce-cde' 'nonce-cde=' 'nonce-cde==' 'nonce-cde==='",
          base::BindOnce([] {
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
          "'sha256-YWJj' 'nonce-cde' 'sha256-QUJD'",
          base::BindOnce([] {
            auto csp = mojom::CSPSourceList::New();
            csp->hashes.push_back(
                mojom::CSPHashSource::New(mojom::CSPHashAlgorithm::SHA256,
                                          std::vector<uint8_t>{'a', 'b', 'c'}));
            csp->hashes.push_back(
                mojom::CSPHashSource::New(mojom::CSPHashAlgorithm::SHA256,
                                          std::vector<uint8_t>{'A', 'B', 'C'}));
            csp->nonces.push_back("cde");
            return csp;
          }),
          "",
      },
      {
          "'none' ",
          base::BindOnce([] { return mojom::CSPSourceList::New(); }),
          "",
      },
      {
          "'none' 'self'",
          base::BindOnce([] {
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
          base::BindOnce([] {
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
          "'none' 'report-sample'",
          base::BindOnce([] {
            auto csp = mojom::CSPSourceList::New();
            csp->report_sample = true;
            return csp;
          }),
          "",
      },
      {
          "'none' 'self' 'report-sample'",
          base::BindOnce([] {
            auto csp = mojom::CSPSourceList::New();
            csp->allow_self = true;
            csp->report_sample = true;
            return csp;
          }),
          "The Content-Security-Policy directive 'script-src' contains the "
          "keyword 'none' alongside with other source expressions. The keyword "
          "'none' must be the only source expression in the directive value, "
          "otherwise it is ignored.",
      },
      {
          "'self'",
          base::BindOnce([] {
            auto csp = mojom::CSPSourceList::New();
            csp->allow_self = true;
            return csp;
          }),
      },
      {
          "'wrong' *",
          base::BindOnce([] {
            auto csp = mojom::CSPSourceList::New();
            csp->allow_star = true;
            return csp;
          }),
          "The source list for the Content Security Policy directive "
          "'script-src' contains an invalid source: ''wrong''. It will be "
          "ignored.",
      },
      {
          "'wrong' 'unsafe-inline'",
          base::BindOnce([] {
            auto csp = mojom::CSPSourceList::New();
            csp->allow_inline = true;
            return csp;
          }),
          "The source list for the Content Security Policy directive "
          "'script-src' contains an invalid source: ''wrong''. It will be "
          "ignored.",
      },
      {
          "'wrong' 'unsafe-eval'",
          base::BindOnce([] {
            auto csp = mojom::CSPSourceList::New();
            csp->allow_eval = true;
            return csp;
          }),
          "The source list for the Content Security Policy directive "
          "'script-src' contains an invalid source: ''wrong''. It will be "
          "ignored.",
      },
      {
          "'wrong' 'wasm-eval'",
          base::BindOnce([] {
            auto csp = mojom::CSPSourceList::New();
            csp->allow_wasm_eval = true;
            return csp;
          }),
          "The source list for the Content Security Policy directive "
          "'script-src' contains an invalid source: ''wrong''. It will be "
          "ignored.",
      },
      {
          "'wrong' 'wasm-unsafe-eval'",
          base::BindOnce([] {
            auto csp = mojom::CSPSourceList::New();
            csp->allow_wasm_unsafe_eval = true;
            return csp;
          }),
          "The source list for the Content Security Policy directive "
          "'script-src' contains an invalid source: ''wrong''. It will be "
          "ignored.",
      },
      {
          "'wrong' 'strict-dynamic'",
          base::BindOnce([] {
            auto csp = mojom::CSPSourceList::New();
            csp->allow_dynamic = true;
            return csp;
          }),
          "The source list for the Content Security Policy directive "
          "'script-src' contains an invalid source: ''wrong''. It will be "
          "ignored.",
      },
      {
          "'wrong' 'unsafe-hashes'",
          base::BindOnce([] {
            auto csp = mojom::CSPSourceList::New();
            csp->allow_unsafe_hashes = true;
            return csp;
          }),
          "The source list for the Content Security Policy directive "
          "'script-src' contains an invalid source: ''wrong''. It will be "
          "ignored.",
      },
      {
          "'wrong' 'report-sample'",
          base::BindOnce([] {
            auto csp = mojom::CSPSourceList::New();
            csp->report_sample = true;
            return csp;
          }),
          "The source list for the Content Security Policy directive "
          "'script-src' contains an invalid source: ''wrong''. It will be "
          "ignored.",
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
    EXPECT_TRUE(
        std::move(test.expected)
            .Run()
            .Equals(
                policies[0]->directives[mojom::CSPDirectiveName::ScriptSrc]));

    EXPECT_EQ(policies[0]->raw_directives[mojom::CSPDirectiveName::ScriptSrc],
              std::string(
                  base::TrimString(test.directive_value, " ", base::TRIM_ALL)));

    if (!test.expected_error.empty())
      EXPECT_EQ(test.expected_error, policies[0]->parsing_errors[0]);
  }
}

TEST(ContentSecurityPolicy, ParseHash) {
  using Algo = mojom::CSPHashAlgorithm;
  struct TestCase {
    std::string hash;
    Algo expected_algorithm;
    std::vector<uint8_t> expected_hash;
  } cases[] = {
      // For this test, we have the following base64 encoding:
      // abc => YWJj    ABC => QUJD    cd => Y2Q=    abcd => YWJjZA==
      // We also test base64 without padding.
      {"'sha256-YWJj'", Algo::SHA256, {'a', 'b', 'c'}},
      {"'sha256-QUJD'", Algo::SHA256, {'A', 'B', 'C'}},
      {"'sha256", Algo::None, {}},
      {"'sha256-'", Algo::None, {}},
      {"'sha384-YWJj'", Algo::SHA384, {'a', 'b', 'c'}},
      {"'sha512-YWJjZA'", Algo::SHA512, {'a', 'b', 'c', 'd'}},
      {"'sha-YWJj'", Algo::None, {}},
      {"'sha256-*'", Algo::None, {}},
      {"'sha-256-Y2Q'", Algo::SHA256, {'c', 'd'}},
      {"'sha-384-Y2Q='", Algo::SHA384, {'c', 'd'}},
      {"'sha-512-Y2Q='", Algo::SHA512, {'c', 'd'}},
      // "ABCDE" is not valid base64 and should be ignored.
      {"'sha256-ABCDE'", Algo::None, {}},
      {"'sha256--__'", Algo::SHA256, {0xfb, 0xff}},
      {"'sha256-++/'", Algo::SHA256, {0xfb, 0xef}},
      // Other invalid hashes should be ignored.
      {"'sha256-YWJj", Algo::None, {}},
      {"'sha111-YWJj'", Algo::None, {}},
      {"'sha256-ABC('", Algo::None, {}},
  };

  for (auto& test : cases) {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->SetHeader("Content-Security-Policy", "script-src " + test.hash);
    std::vector<mojom::ContentSecurityPolicyPtr> policies;
    AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                        &policies);
    const std::vector<mojom::CSPHashSourcePtr>& hashes =
        policies[0]->directives[mojom::CSPDirectiveName::ScriptSrc]->hashes;
    if (test.expected_algorithm != Algo::None) {
      EXPECT_EQ(1u, hashes.size()) << test.hash << " should parse to one hash";
      EXPECT_EQ(test.expected_algorithm, hashes[0]->algorithm)
          << test.hash << " should have algorithm " << test.expected_algorithm;
      EXPECT_EQ(test.expected_hash, hashes[0]->value)
          << test.hash << " has not been base64decoded correctly";
    } else {
      EXPECT_TRUE(hashes.empty()) << test.hash << " should be an invalid hash";
    }
  }
}

TEST(ContentSecurityPolicy, ParseInlineSpeculationRules) {
  std::vector<mojom::ContentSecurityPolicyPtr> script_src_policies =
      ParseCSP("script-src 'inline-speculation-rules'");
  ASSERT_EQ(1u, script_src_policies.size());
  ASSERT_EQ(1u, script_src_policies[0]->directives.size());
  ASSERT_TRUE(script_src_policies[0]->directives.contains(
      mojom::CSPDirectiveName::ScriptSrc));
  EXPECT_TRUE(script_src_policies[0]
                  ->directives[mojom::CSPDirectiveName::ScriptSrc]
                  ->allow_inline_speculation_rules);
  EXPECT_EQ(0u, script_src_policies[0]->parsing_errors.size());

  std::vector<mojom::ContentSecurityPolicyPtr> script_src_elem_policies =
      ParseCSP("script-src-elem 'inline-speculation-rules'");
  ASSERT_EQ(1u, script_src_elem_policies.size());
  ASSERT_EQ(1u, script_src_elem_policies[0]->directives.size());
  ASSERT_TRUE(script_src_elem_policies[0]->directives.contains(
      mojom::CSPDirectiveName::ScriptSrcElem));
  EXPECT_TRUE(script_src_elem_policies[0]
                  ->directives[mojom::CSPDirectiveName::ScriptSrcElem]
                  ->allow_inline_speculation_rules);
  EXPECT_EQ(0u, script_src_elem_policies[0]->parsing_errors.size());

  std::vector<mojom::ContentSecurityPolicyPtr> img_src_policies =
      ParseCSP("img-src 'inline-speculation-rules'");
  ASSERT_EQ(1u, img_src_policies.size());
  ASSERT_EQ(1u, img_src_policies[0]->directives.size());
  ASSERT_TRUE(img_src_policies[0]->directives.contains(
      mojom::CSPDirectiveName::ImgSrc));
  EXPECT_FALSE(img_src_policies[0]
                   ->directives[mojom::CSPDirectiveName::ImgSrc]
                   ->allow_inline_speculation_rules);
  ASSERT_EQ(1u, img_src_policies[0]->parsing_errors.size());
  EXPECT_EQ(
      "The Content-Security-Policy directive 'img-src' contains "
      "''inline-speculation-rules'' as a source expression that is permitted "
      "only for 'script-src' and 'script-src-elem' directives. It will be "
      "ignored.",
      img_src_policies[0]->parsing_errors[0]);
}

TEST(ContentSecurityPolicy, IsValidRequiredCSPAttr) {
  struct TestCase {
    const char* csp;
    bool expected;
    std::string expected_error;
  } cases[] = {
      {"  script-src 'none'  https://www.google.com ;;  ; invalid-directive "
       "invalid-value ;",
       true, ""},
      {"script-src 'none'; report-uri https://www.example.com", false,
       "The csp attribute cannot contain the directives 'report-to' "
       "or 'report-uri'."},
  };

  for (auto& test : cases) {
    SCOPED_TRACE(test.csp);
    std::vector<mojom::ContentSecurityPolicyPtr> csp;
    auto required_csp_headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    required_csp_headers->SetHeader("Content-Security-Policy", test.csp);
    AddContentSecurityPolicyFromHeaders(*required_csp_headers,
                                        GURL("https://example.com/"), &csp);

    // Overwrite the header_value artificially. At the moment, our header parser
    // takes already care of some parts (like removing commas). But we want to
    // be sure that header values with commas or other invalid header values are
    // blocked by our validation mechanism anyway.
    csp[0]->header->header_value = test.csp;
    std::string out;
    EXPECT_EQ(test.expected, IsValidRequiredCSPAttr(csp, nullptr, out));
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
    EXPECT_EQ(test.expected, Subsumes(*required_csp[0], returned_csp))
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
    EXPECT_EQ(Subsumes(*policy_a[0], policies_b), test.expected)
        << csp_a << " should " << (test.expected ? "" : "not ") << "subsume "
        << test.policies;

    if (!policies_b.empty()) {
      // Check if first policy of `listB` subsumes `A`.
      EXPECT_EQ(Subsumes(*policies_b[0], policy_a),
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
    EXPECT_EQ(Subsumes(*policy_a[0], policies_b), test.expected)
        << test.policy_a << " should " << (test.expected ? "" : "not ")
        << "subsume " << test.policies_b;
  }
}

TEST(ContentSecurityPolicy, InvalidPolicyInReportOnlySandbox) {
  mojom::ContentSecurityPolicyPtr policy = ParseOneCspReportOnly("sandbox");

  EXPECT_EQ(mojom::WebSandboxFlags::kNone, policy->sandbox);
  ASSERT_EQ(1u, policy->parsing_errors.size());
  EXPECT_EQ(
      "The Content Security Policy directive 'sandbox' is ignored when "
      "delivered in a report-only policy.",
      policy->parsing_errors[0]);
}

TEST(ContentSecurityPolicy, InvalidPolicyInReportOnlyUpgradeInsecureRequest) {
  mojom::ContentSecurityPolicyPtr policy =
      ParseOneCspReportOnly("upgrade-insecure-requests");

  EXPECT_FALSE(policy->upgrade_insecure_requests);
  ASSERT_EQ(1u, policy->parsing_errors.size());
  EXPECT_EQ(
      "The Content Security Policy directive 'upgrade-insecure-requests' is "
      "ignored when delivered in a report-only policy.",
      policy->parsing_errors[0]);
}

TEST(ContentSecurityPolicy, InvalidPolicyInReportTreatAsPublicAddress) {
  mojom::ContentSecurityPolicyPtr policy =
      ParseOneCspReportOnly("treat-as-public-address");

  EXPECT_FALSE(policy->treat_as_public_address);
  ASSERT_EQ(1u, policy->parsing_errors.size());
  EXPECT_EQ(
      "The Content Security Policy directive 'treat-as-public-address' is "
      "ignored when delivered in a report-only policy.",
      policy->parsing_errors[0]);
}

TEST(ContentSecurityPolicy, InvalidPolicyInMetaFrameAncestors) {
  std::vector<mojom::ContentSecurityPolicyPtr> policies =
      ParseContentSecurityPolicies(
          "frame-ancestors https://www.example.org; script-src 'none'",
          mojom::ContentSecurityPolicyType::kEnforce,
          mojom::ContentSecurityPolicySource::kMeta,
          GURL("https://www.example.org"));

  ASSERT_EQ(1u, policies.size());
  mojom::ContentSecurityPolicyPtr& policy = policies[0];
  EXPECT_EQ(1u, policy->directives.size());

  auto& script_src = policy->directives[mojom::CSPDirectiveName::ScriptSrc];
  EXPECT_EQ(script_src->sources.size(), 0U);

  ASSERT_EQ(1u, policy->parsing_errors.size());
  EXPECT_EQ(
      "The Content Security Policy directive 'frame-ancestors' is ignored when "
      "delivered via a <meta> element.",
      policy->parsing_errors[0]);
}

TEST(ContentSecurityPolicy, InvalidPolicyInMetaReportUri) {
  std::vector<mojom::ContentSecurityPolicyPtr> policies =
      ParseContentSecurityPolicies(
          "report-uri https://www.example.org; script-src 'none'",
          mojom::ContentSecurityPolicyType::kEnforce,
          mojom::ContentSecurityPolicySource::kMeta,
          GURL("https://www.example.org"));

  ASSERT_EQ(1u, policies.size());
  mojom::ContentSecurityPolicyPtr& policy = policies[0];
  EXPECT_EQ(1u, policy->directives.size());

  auto& script_src = policy->directives[mojom::CSPDirectiveName::ScriptSrc];
  EXPECT_EQ(script_src->sources.size(), 0U);

  ASSERT_EQ(1u, policy->parsing_errors.size());
  EXPECT_EQ(
      "The Content Security Policy directive 'report-uri' is ignored when "
      "delivered via a <meta> element.",
      policy->parsing_errors[0]);
}

TEST(ContentSecurityPolicy, InvalidPolicyInMetaSandbox) {
  std::vector<mojom::ContentSecurityPolicyPtr> policies =
      ParseContentSecurityPolicies("sandbox; script-src 'none'",
                                   mojom::ContentSecurityPolicyType::kEnforce,
                                   mojom::ContentSecurityPolicySource::kMeta,
                                   GURL("https://www.example.org"));

  ASSERT_EQ(1u, policies.size());
  mojom::ContentSecurityPolicyPtr& policy = policies[0];
  EXPECT_EQ(mojom::WebSandboxFlags::kNone, policy->sandbox);
  EXPECT_EQ(1u, policy->directives.size());

  auto& script_src = policy->directives[mojom::CSPDirectiveName::ScriptSrc];
  EXPECT_EQ(script_src->sources.size(), 0U);

  ASSERT_EQ(1u, policy->parsing_errors.size());
  EXPECT_EQ(
      "The Content Security Policy directive 'sandbox' is ignored when "
      "delivered via a <meta> element.",
      policy->parsing_errors[0]);
}

TEST(ContentSecurityPolicy, InvalidPolicyInMetaTreatAsPublicAddress) {
  std::vector<mojom::ContentSecurityPolicyPtr> policies =
      ParseContentSecurityPolicies("treat-as-public-address; script-src 'none'",
                                   mojom::ContentSecurityPolicyType::kEnforce,
                                   mojom::ContentSecurityPolicySource::kMeta,
                                   GURL("https://www.example.org"));

  ASSERT_EQ(1u, policies.size());
  mojom::ContentSecurityPolicyPtr& policy = policies[0];
  EXPECT_FALSE(policy->treat_as_public_address);
  EXPECT_EQ(1u, policy->directives.size());

  auto& script_src = policy->directives[mojom::CSPDirectiveName::ScriptSrc];
  EXPECT_EQ(script_src->sources.size(), 0U);

  ASSERT_EQ(1u, policy->parsing_errors.size());
  EXPECT_EQ(
      "The Content Security Policy directive 'treat-as-public-address' is "
      "ignored when delivered via a <meta> element.",
      policy->parsing_errors[0]);
}

TEST(ContentSecurityPolicy, AllowsBlanketEnforcementOfRequiredCSP) {
  struct TestCase {
    const char* name;
    const char* request_origin;
    const char* response_origin;
    const char* allow_csp_from;
    bool expected_result;
    const char* expected_self_origin;
  } cases[] = {
      {
          "About scheme allows",
          "http://example.com",
          "about://me",
          nullptr,
          true,
          "http://example.com",
      },
      {
          "File scheme allows",
          "http://example.com",
          "file://me",
          nullptr,
          true,
          "http://example.com",
      },
      {
          "Data scheme allows",
          "http://example.com",
          "data://me",
          nullptr,
          true,
          "http://example.com",
      },
      {
          "Filesystem scheme allows",
          "http://example.com",
          "filesystem://me",
          nullptr,
          true,
          "http://example.com",
      },
      {
          "Blob scheme allows",
          "http://example.com",
          "blob://me",
          nullptr,
          true,
          "http://example.com",
      },
      {
          "Same origin does not allow",
          "http://example.com",
          "http://example.com",
          nullptr,
          false,
      },
      {
          "Same origin with right header allows",
          "http://example.com",
          "http://example.com",
          "http://example.com",
          true,
          "http://example.com",
      },
      {
          "Same origin with wrong header does not allow",
          "http://example.com",
          "http://example.com",
          "http://not-example.com",
          false,
      },
      {
          "Different origin does not allow",
          "http://example.com",
          "http://not.example.com",
          nullptr,
          false,
      },
      {
          "Different origin with right header allows",
          "http://example.com",
          "http://not-example.com",
          "http://example.com",
          true,
          "http://not-example.com",
      },
      {
          "Different origin with right header 2 allows",
          "http://example.com",
          "http://not-example.com",
          "http://example.com/",
          true,
          "http://not-example.com",
      },
      {
          "Different origin with wrong header does not allow",
          "http://example.com",
          "http://not-example.com",
          "http://not-example.com",
          false,
      },
      {
          "Wildcard header allows",
          "http://example.com",
          "http://not-example.com",
          "*",
          true,
          "http://not-example.com",
      },
      {
          "Malformed header does not allow",
          "http://example.com",
          "http://not-example.com",
          "*; http://example.com",
          false,
      },
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.name);
    auto headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    if (test.allow_csp_from)
      headers->AddHeader("allow-csp-from", test.allow_csp_from);
    auto allow_csp_from = network::ParseAllowCSPFromHeader(*headers);

    auto required_csp = mojom::ContentSecurityPolicy::New();
    bool actual = AllowsBlanketEnforcementOfRequiredCSP(
        url::Origin::Create(GURL(test.request_origin)),
        GURL(test.response_origin), allow_csp_from.get(), required_csp);
    EXPECT_EQ(test.expected_result, actual);
    if (test.expected_self_origin) {
      GURL expected_self_origin(test.expected_self_origin);
      EXPECT_EQ(expected_self_origin.scheme(),
                required_csp->self_origin->scheme);
      EXPECT_EQ(expected_self_origin.host(), required_csp->self_origin->host);
      EXPECT_EQ(expected_self_origin.EffectiveIntPort(),
                required_csp->self_origin->port);
    }
  }
}

TEST(ContentSecurityPolicy, FencedFrameSrcFallback) {
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
    EXPECT_FALSE(CheckContentSecurityPolicy(
        policy, CSPDirectiveName::FencedFrameSrc, GURL("http://b.com"), GURL(),
        false, &context, SourceLocation(), false));
    ASSERT_EQ(1u, context.violations().size());
    const char kConsoleMessage[] =
        "Refused to frame 'http://b.com/' as a fenced frame because it "
        "violates the following Content Security Policy directive: "
        "\"default-src http://a.com\". Note that 'fenced-frame-src' was not "
        "explicitly set, so 'default-src' is used as a fallback.\n";
    EXPECT_EQ(kConsoleMessage, context.violations()[0]->console_message);
    EXPECT_TRUE(CheckContentSecurityPolicy(
        policy, CSPDirectiveName::FencedFrameSrc, GURL("http://a.com"), GURL(),
        false, &context, SourceLocation(), false));
  }
  {
    CSPContextTest context;
    auto policy = EmptyCSP();
    policy->directives[CSPDirectiveName::ChildSrc] = allow_host("a.com");
    EXPECT_FALSE(CheckContentSecurityPolicy(
        policy, CSPDirectiveName::FencedFrameSrc, GURL("http://b.com"), GURL(),
        false, &context, SourceLocation(), false));
    ASSERT_EQ(1u, context.violations().size());
    const char kConsoleMessage[] =
        "Refused to frame 'http://b.com/' as a fenced frame because it "
        "violates the following Content Security Policy directive: "
        "\"child-src http://a.com\". Note that 'fenced-frame-src' was not "
        "explicitly set, so 'child-src' is used as a fallback.\n";
    EXPECT_EQ(kConsoleMessage, context.violations()[0]->console_message);
    EXPECT_TRUE(CheckContentSecurityPolicy(
        policy, CSPDirectiveName::FencedFrameSrc, GURL("http://a.com"), GURL(),
        false, &context, SourceLocation(), false));
  }
  {
    CSPContextTest context;
    auto policy = EmptyCSP();
    policy->directives[CSPDirectiveName::FrameSrc] = allow_host("a.com");

    EXPECT_FALSE(CheckContentSecurityPolicy(
        policy, CSPDirectiveName::FencedFrameSrc, GURL("http://b.com"), GURL(),
        false, &context, SourceLocation(), false));
    ASSERT_EQ(1u, context.violations().size());
    const char kConsoleMessage[] =
        "Refused to frame 'http://b.com/' as a fenced frame because it "
        "violates the following Content Security Policy directive: "
        "\"frame-src http://a.com\". Note that 'fenced-frame-src' was not "
        "explicitly set, so 'frame-src' is used as a fallback.\n";
    EXPECT_EQ(kConsoleMessage, context.violations()[0]->console_message);
    EXPECT_TRUE(CheckContentSecurityPolicy(
        policy, CSPDirectiveName::FencedFrameSrc, GURL("http://a.com"), GURL(),
        false, &context, SourceLocation(), false));
  }
  {
    CSPContextTest context;
    auto policy = EmptyCSP();
    policy->directives[CSPDirectiveName::FencedFrameSrc] = allow_host("a.com");
    policy->directives[CSPDirectiveName::FrameSrc] = allow_host("b.com");
    EXPECT_TRUE(CheckContentSecurityPolicy(
        policy, CSPDirectiveName::FencedFrameSrc, GURL("http://a.com"), GURL(),
        false, &context, SourceLocation(), false));
    EXPECT_FALSE(CheckContentSecurityPolicy(
        policy, CSPDirectiveName::FencedFrameSrc, GURL("http://b.com"), GURL(),
        false, &context, SourceLocation(), false));
    ASSERT_EQ(1u, context.violations().size());
    const char kConsoleMessage[] =
        "Refused to frame 'http://b.com/' as a fenced frame because it "
        "violates the following Content Security Policy directive: "
        "\"fenced-frame-src http://a.com\".\n";
    EXPECT_EQ(kConsoleMessage, context.violations()[0]->console_message);
  }
}

TEST(ContentSecurityPolicy, FencedFrameSrcOpaqueURL) {
  CSPContextTest context;
  auto policy = EmptyCSP();
  policy->directives[CSPDirectiveName::FencedFrameSrc] =
      mojom::CSPSourceList::New();
  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FencedFrameSrc, GURL("https://a.com"), GURL(),
      /*has_followed_redirect=*/false, &context, SourceLocation(),
      /*is_form_submission=*/false,
      /*is_opaque_fenced_frame=*/true));
  ASSERT_EQ(1u, context.violations().size());
  const char kConsoleMessage[] =
      "Refused to frame 'urn:uuid' as a fenced frame because it violates the "
      "following Content Security Policy directive: \"fenced-frame-src "
      "'none'\".\n";
  EXPECT_EQ(kConsoleMessage, context.violations()[0]->console_message);
}

}  // namespace network
