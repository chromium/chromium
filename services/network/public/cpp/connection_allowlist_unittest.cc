// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/connection_allowlist.h"

#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/connection_allowlist_parser.h"
#include "services/network/public/mojom/connection_allowlist.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

namespace {

const GURL kExampleURL = GURL("https://site.example/");
constexpr char kSerializedExampleOrigin[] = "https://site.example";

}  // namespace

class ConnectionAllowlistParserTest : public testing::Test {
 protected:
  ConnectionAllowlistParserTest() = default;

  scoped_refptr<net::HttpResponseHeaders> GetHeaders(const char* enforced,
                                                     const char* report_only) {
    auto builder =
        net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200");
    if (enforced) {
      builder.AddHeader("Connection-Allowlist", enforced);
    }
    if (report_only) {
      builder.AddHeader("Connection-Allowlist-Report-Only", report_only);
    }
    return builder.Build();
  }

  const GURL& url() { return kExampleURL; }
};

TEST_F(ConnectionAllowlistParserTest, NoHeaders) {
  auto headers = GetHeaders(nullptr, nullptr);
  ConnectionAllowlists result =
      ParseConnectionAllowlistsFromHeaders(*headers, url());
  EXPECT_FALSE(result.enforced);
  EXPECT_FALSE(result.report_only);
}

TEST_F(ConnectionAllowlistParserTest, EmptyHeaders) {
  auto headers = GetHeaders("", "");
  ConnectionAllowlists result =
      ParseConnectionAllowlistsFromHeaders(*headers, url());
  EXPECT_FALSE(result.enforced);
  EXPECT_FALSE(result.report_only);
}

TEST_F(ConnectionAllowlistParserTest, MalformedHeader) {
  struct {
    const char* value;
    mojom::ConnectionAllowlistIssue issue;
  } cases[] = {
      // Non-list
      {"dictionary=value", mojom::ConnectionAllowlistIssue::kInvalidHeader},

      // List with non-Inner List
      {"1", mojom::ConnectionAllowlistIssue::kItemNotInnerList},
      {"1.1", mojom::ConnectionAllowlistIssue::kItemNotInnerList},
      {"\"string\"", mojom::ConnectionAllowlistIssue::kItemNotInnerList},
      {"token", mojom::ConnectionAllowlistIssue::kItemNotInnerList},
      {":lS/LFS0xbMKoQ0JWBZySc9ChRIZMbAuWO69kAVCb12k=:",
       mojom::ConnectionAllowlistIssue::kItemNotInnerList},
      {"?0", mojom::ConnectionAllowlistIssue::kItemNotInnerList},

      // Invalid allowlist type:
      {" (1)", mojom::ConnectionAllowlistIssue::kInvalidAllowlistItemType},
      {" (1.1)", mojom::ConnectionAllowlistIssue::kInvalidAllowlistItemType},
      {" (invalid-token)",
       mojom::ConnectionAllowlistIssue::kInvalidAllowlistItemType},
      {"( :lS/LFS0xbMKoQ0JWBZySc9ChRIZMbAuWO69kAVCb12k=:)",
       mojom::ConnectionAllowlistIssue::kInvalidAllowlistItemType},
      {" (?0)", mojom::ConnectionAllowlistIssue::kInvalidAllowlistItemType},
      {" (\"response-origin\")",
       mojom::ConnectionAllowlistIssue::kInvalidAllowlistItemType},

      // Invalid reporting endpoint type:
      {"(); report-to=1",
       mojom::ConnectionAllowlistIssue::kReportingEndpointNotToken},
      {"(); report-to=1.1",
       mojom::ConnectionAllowlistIssue::kReportingEndpointNotToken},
      {"(); report-to=\"string\"",
       mojom::ConnectionAllowlistIssue::kReportingEndpointNotToken},
      {"(); report-to=:lS/LFS0xbMKoQ0JWBZySc9ChRIZMbAuWO69kAVCb12k=:",
       mojom::ConnectionAllowlistIssue::kReportingEndpointNotToken},
      {"(); report-to",
       mojom::ConnectionAllowlistIssue::kReportingEndpointNotToken},
      {"(); report-to=?1",
       mojom::ConnectionAllowlistIssue::kReportingEndpointNotToken},
      {"(); report-to=?0",
       mojom::ConnectionAllowlistIssue::kReportingEndpointNotToken},

      // Note: we're not testing dates (`@12345`) or display strings
      // (`%"display"`) because our structured field parser doesn't yet
      // support those types.
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test.value << "`");
    // Enforced header:
    {
      auto headers = GetHeaders(test.value, nullptr);
      ConnectionAllowlists result =
          ParseConnectionAllowlistsFromHeaders(*headers, url());
      ASSERT_TRUE(result.enforced);
      EXPECT_FALSE(result.report_only);
      EXPECT_TRUE(result.enforced->allowlist.empty());
      ASSERT_EQ(1u, result.enforced->issues.size());
      EXPECT_EQ(test.issue, result.enforced->issues[0]);
    }

    // Report-Only header:
    {
      auto headers = GetHeaders(nullptr, test.value);
      ConnectionAllowlists result =
          ParseConnectionAllowlistsFromHeaders(*headers, url());
      EXPECT_FALSE(result.enforced);
      ASSERT_TRUE(result.report_only);
      EXPECT_TRUE(result.report_only->allowlist.empty());
      ASSERT_EQ(1u, result.report_only->issues.size());
      EXPECT_EQ(test.issue, result.report_only->issues[0]);
    }

    // Both headers:
    {
      auto headers = GetHeaders(test.value, test.value);
      ConnectionAllowlists result =
          ParseConnectionAllowlistsFromHeaders(*headers, url());
      ASSERT_TRUE(result.enforced);
      ASSERT_TRUE(result.report_only);
      EXPECT_TRUE(result.enforced->allowlist.empty());
      EXPECT_TRUE(result.report_only->allowlist.empty());
      ASSERT_EQ(1u, result.enforced->issues.size());
      ASSERT_EQ(1u, result.report_only->issues.size());
      EXPECT_EQ(test.issue, result.enforced->issues[0]);
      EXPECT_EQ(test.issue, result.report_only->issues[0]);
    }
  }
}

TEST_F(ConnectionAllowlistParserTest, ValidAllowlists) {
  struct {
    const char* value;
    std::vector<std::string> allowlist;
  } cases[] = {
      // Empty:
      {"()", {}},

      // Single item:
      {"(\"https://site.example\")", {"https://site.example"}},
      {"(\"https://site.example/*\")", {"https://site.example/*"}},
      {"(\"https://*.site.example/*\")", {"https://*.site.example/*"}},
      {"(\"https://site.example/[0-9]+\")", {"https://site.example/[0-9]+"}},
      {"(response-origin)", {kSerializedExampleOrigin}},

      // Multiple items:
      {"(\"https://*.site.example/\" \"https://other.example/\")",
       {"https://*.site.example/", "https://other.example/"}},
      {"(\"https://other.example/\" \"https://*.site.example/\")",
       {"https://other.example/", "https://*.site.example/"}},
      {"(response-origin \"https://other.example/\")",
       {kSerializedExampleOrigin, "https://other.example/"}},
      {"(\"https://other.example/\" response-origin)",
       {"https://other.example/", kSerializedExampleOrigin}},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test.value << "`");

    // Enforced header:
    {
      auto headers = GetHeaders(test.value, nullptr);
      ConnectionAllowlists result =
          ParseConnectionAllowlistsFromHeaders(*headers, url());
      ASSERT_TRUE(result.enforced);
      EXPECT_FALSE(result.report_only);

      ASSERT_EQ(0u, result.enforced->issues.size());
      EXPECT_EQ(result.enforced->allowlist, test.allowlist);
    }

    // Report-Only header:
    {
      auto headers = GetHeaders(nullptr, test.value);
      ConnectionAllowlists result =
          ParseConnectionAllowlistsFromHeaders(*headers, url());
      EXPECT_FALSE(result.enforced);
      ASSERT_TRUE(result.report_only);

      ASSERT_EQ(0u, result.report_only->issues.size());
      EXPECT_EQ(result.report_only->allowlist, test.allowlist);
    }

    // Both headers:
    {
      auto headers = GetHeaders(test.value, test.value);
      ConnectionAllowlists result =
          ParseConnectionAllowlistsFromHeaders(*headers, url());
      ASSERT_TRUE(result.enforced);
      ASSERT_TRUE(result.report_only);

      ASSERT_EQ(0u, result.enforced->issues.size());
      ASSERT_EQ(0u, result.report_only->issues.size());
      EXPECT_EQ(result.enforced->allowlist, test.allowlist);
      EXPECT_EQ(result.report_only->allowlist, test.allowlist);
    }
  }
}

TEST_F(ConnectionAllowlistParserTest, ValidReportToEndpoint) {
  auto headers = GetHeaders("();report-to=endpoint", nullptr);
  ConnectionAllowlists result =
      ParseConnectionAllowlistsFromHeaders(*headers, url());
  ASSERT_TRUE(result.enforced);
  EXPECT_EQ(0u, result.enforced->issues.size());
  EXPECT_TRUE(result.enforced->allowlist.empty());
  ASSERT_TRUE(result.enforced->reporting_endpoint.has_value());
  EXPECT_EQ("endpoint", result.enforced->reporting_endpoint.value());
  EXPECT_FALSE(result.report_only);
}

TEST_F(ConnectionAllowlistParserTest, MultipleLists) {
  auto headers =
      GetHeaders("(\"https://a.example\"), (\"https://b.example\")", nullptr);
  ConnectionAllowlists result =
      ParseConnectionAllowlistsFromHeaders(*headers, url());
  ASSERT_TRUE(result.enforced);
  ASSERT_EQ(1u, result.enforced->issues.size());
  EXPECT_EQ(mojom::ConnectionAllowlistIssue::kMoreThanOneList,
            result.enforced->issues[0]);
  ASSERT_EQ(1u, result.enforced->allowlist.size());
  EXPECT_EQ("https://a.example", result.enforced->allowlist[0]);
  EXPECT_FALSE(result.report_only);
}

}  // namespace network
