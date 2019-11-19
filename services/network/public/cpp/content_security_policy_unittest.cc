// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_security_policy.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/third_party/mozilla/url_parse.h"

namespace network {

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
  bool is_valid;
  ExpectedResult expected_result;

  TestData(const std::string& header, ExpectedResult expected_result)
      : header(header), is_valid(true), expected_result(expected_result) {}
  TestData(const std::string& header) : header(header), is_valid(false) {}
};

static void TestCSPParser(const std::string& header,
                          const ExpectedResult* expected_result) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  headers->AddHeader("Content-Security-Policy: frame-ancestors " + header);
  ContentSecurityPolicy policy;
  policy.Parse(GURL("https://example.com/"), *headers);

  if (!expected_result) {
    EXPECT_FALSE(policy.content_security_policy_ptr());
    return;
  }
  auto& frame_ancestors = policy.content_security_policy_ptr()->frame_ancestors;
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

      // Invalid 'none'
      {"example.com 'none'"},

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
      {"about:blank"},
      {""},
  };

  for (auto& test : test_data)
    TestCSPParser(test.header, test.is_valid ? &test.expected_result : nullptr);
}

TEST(ContentSecurityPolicy, ParseMultipleDirectives) {
  // First directive is valid, second one is ignored.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->AddHeader(
        "Content-Security-Policy: frame-ancestors example.com; other_directive "
        "value; frame-ancestors example.org");
    ContentSecurityPolicy policy;
    policy.Parse(GURL("https://example.com/"), *headers);

    auto& frame_ancestors =
        policy.content_security_policy_ptr()->frame_ancestors;
    EXPECT_EQ(frame_ancestors->sources.size(), 1U);
    EXPECT_EQ(frame_ancestors->sources[0]->scheme, "");
    EXPECT_EQ(frame_ancestors->sources[0]->host, "example.com");
    EXPECT_EQ(frame_ancestors->sources[0]->port, url::PORT_UNSPECIFIED);
    EXPECT_EQ(frame_ancestors->sources[0]->path, "");
    EXPECT_EQ(frame_ancestors->sources[0]->is_host_wildcard, false);
    EXPECT_EQ(frame_ancestors->sources[0]->is_port_wildcard, false);
    EXPECT_EQ(frame_ancestors->allow_self, false);
  }

  // Skip the first directive, which is not frame-ancestors.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->AddHeader(
        "Content-Security-Policy: other_directive value; frame-ancestors "
        "example.org");
    ContentSecurityPolicy policy;
    policy.Parse(GURL("https://example.com/"), *headers);

    auto& frame_ancestors =
        policy.content_security_policy_ptr()->frame_ancestors;
    EXPECT_EQ(frame_ancestors->sources.size(), 1U);
    EXPECT_EQ(frame_ancestors->sources[0]->scheme, "");
    EXPECT_EQ(frame_ancestors->sources[0]->host, "example.org");
    EXPECT_EQ(frame_ancestors->sources[0]->port, url::PORT_UNSPECIFIED);
    EXPECT_EQ(frame_ancestors->sources[0]->path, "");
    EXPECT_EQ(frame_ancestors->sources[0]->is_host_wildcard, false);
    EXPECT_EQ(frame_ancestors->sources[0]->is_port_wildcard, false);
    EXPECT_EQ(frame_ancestors->allow_self, false);
    EXPECT_EQ(frame_ancestors->allow_star, false);
  }

  // Multiple CSP headers with multiple frame-ancestors directives present. Only
  // the first one is considered.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->AddHeader("Content-Security-Policy: frame-ancestors example.com");
    headers->AddHeader("Content-Security-Policy: frame-ancestors example.org");
    ContentSecurityPolicy policy;
    policy.Parse(GURL("https://example.com/"), *headers);

    auto& frame_ancestors =
        policy.content_security_policy_ptr()->frame_ancestors;
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

  // Multiple CSP headers separated by ',' (RFC2616 section 4.2).
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->AddHeader(
        "Content-Security-Policy: other_directive value, frame-ancestors "
        "example.org");
    ContentSecurityPolicy policy;
    policy.Parse(GURL("https://example.com/"), *headers);

    auto& frame_ancestors =
        policy.content_security_policy_ptr()->frame_ancestors;
    EXPECT_EQ(frame_ancestors->sources.size(), 1U);
    EXPECT_EQ(frame_ancestors->sources[0]->scheme, "");
    EXPECT_EQ(frame_ancestors->sources[0]->host, "example.org");
    EXPECT_EQ(frame_ancestors->sources[0]->port, url::PORT_UNSPECIFIED);
    EXPECT_EQ(frame_ancestors->sources[0]->path, "");
    EXPECT_EQ(frame_ancestors->sources[0]->is_host_wildcard, false);
    EXPECT_EQ(frame_ancestors->sources[0]->is_port_wildcard, false);
    EXPECT_EQ(frame_ancestors->allow_self, false);
    EXPECT_EQ(frame_ancestors->allow_star, false);
  }

  // Multiple CSP headers separated by ',', with multiple frame-ancestors
  // directives present. Only the first one is considered.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->AddHeader(
        "Content-Security-Policy: frame-ancestors example.com, frame-ancestors "
        "example.org");
    ContentSecurityPolicy policy;
    policy.Parse(GURL("https://example.com/"), *headers);

    auto& frame_ancestors =
        policy.content_security_policy_ptr()->frame_ancestors;
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

  // Both frame-ancestors and report-to directives present.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->AddHeader(
        "Content-Security-Policy: report-to http://example.com/report; "
        "frame-ancestors example.com");
    ContentSecurityPolicy policy;
    policy.Parse(GURL("https://example.com/"), *headers);

    auto& report_endpoints =
        policy.content_security_policy_ptr()->report_endpoints;
    EXPECT_EQ(report_endpoints.size(), 1U);
    EXPECT_EQ(report_endpoints[0], "http://example.com/report");
    EXPECT_TRUE(policy.content_security_policy_ptr()->use_reporting_api);

    auto& frame_ancestors =
        policy.content_security_policy_ptr()->frame_ancestors;
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

TEST(ContentSecurityPolicy, ParseReportEndpoint) {
  // report-uri directive.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->AddHeader(
        "Content-Security-Policy: report-uri http://example.com/report");
    ContentSecurityPolicy policy;
    policy.Parse(GURL("https://example.com/"), *headers);

    auto& report_endpoints =
        policy.content_security_policy_ptr()->report_endpoints;
    EXPECT_EQ(report_endpoints.size(), 1U);
    EXPECT_EQ(report_endpoints[0], "http://example.com/report");
    EXPECT_FALSE(policy.content_security_policy_ptr()->use_reporting_api);
  }

  // report-to directive.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->AddHeader(
        "Content-Security-Policy: report-to http://example.com/report");
    ContentSecurityPolicy policy;
    policy.Parse(GURL("https://example.com/"), *headers);

    auto& report_endpoints =
        policy.content_security_policy_ptr()->report_endpoints;
    EXPECT_EQ(report_endpoints.size(), 1U);
    EXPECT_EQ(report_endpoints[0], "http://example.com/report");
    EXPECT_TRUE(policy.content_security_policy_ptr()->use_reporting_api);
  }

  // Multiple directives. The report-to directive always takes priority.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->AddHeader(
        "Content-Security-Policy: report-uri http://example.com/report1");
    headers->AddHeader(
        "Content-Security-Policy: report-uri http://example.com/report2");
    headers->AddHeader(
        "Content-Security-Policy: report-to http://example.com/report3");
    ContentSecurityPolicy policy;
    policy.Parse(GURL("https://example.com/"), *headers);

    auto& report_endpoints =
        policy.content_security_policy_ptr()->report_endpoints;
    EXPECT_EQ(report_endpoints.size(), 1U);
    EXPECT_EQ(report_endpoints[0], "http://example.com/report3");
    EXPECT_TRUE(policy.content_security_policy_ptr()->use_reporting_api);
  }
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->AddHeader(
        "Content-Security-Policy: report-to http://example.com/report1");
    headers->AddHeader(
        "Content-Security-Policy: report-uri http://example.com/report2");
    ContentSecurityPolicy policy;
    policy.Parse(GURL("https://example.com/"), *headers);

    auto& report_endpoints =
        policy.content_security_policy_ptr()->report_endpoints;
    EXPECT_EQ(report_endpoints.size(), 1U);
    EXPECT_EQ(report_endpoints[0], "http://example.com/report1");
    EXPECT_TRUE(policy.content_security_policy_ptr()->use_reporting_api);
  }
}

}  // namespace network
