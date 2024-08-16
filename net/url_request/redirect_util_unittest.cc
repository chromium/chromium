// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/redirect_util.h"

#include <string>

#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {
namespace {

TEST(RedirectUtilTest, UpdateHttpRequest) {
  const GURL original_url("https://www.example.com/test.php");
  const char kContentLengthValue[] = "100";
  const char kContentTypeValue[] = "text/plain; charset=utf-8";
  const char kContentEncoding[] = "Content-Encoding";
  const char kContentEncodingValue[] = "gzip";
  const char kContentLanguage[] = "Content-Language";
  const char kContentLanguageValue[] = "tlh";
  const char kContentLocation[] = "Content-Location";
  const char kContentLocationValue[] = "https://somewhere.test/";
  const char kCustomHeader[] = "Custom-Header-For-Test";
  const char kCustomHeaderValue[] = "custom header value";

  struct TestCase {
    const char* original_method;
    const char* new_method;
    const char* new_url;
    const struct {
      const char* name;
      const char* value;
    } modified_headers[2];
    bool expected_should_clear_upload;
    // std::nullopt if the origin header should not exist
    std::optional<const char*> expected_origin_header;
  };
  const TestCase kTests[] = {
      {
          "POST" /* original_method */,
          "POST" /* new_method */,
          "https://www.example.com/redirected.php" /* new_url */,
          {{"Header1", "Value1"}, {"Header2", "Value2"}} /* modified_headers */,
          false /* expected_should_clear_upload */,
          "https://origin.example.com" /* expected_origin_header */
      },
      {
          "POST" /* original_method */,
          "GET" /* new_method */,
          "https://www.example.com/redirected.php" /* new_url */,
          {{"Header1", "Value1"}, {"Header2", "Value2"}} /* modified_headers */,
          true /* expected_should_clear_upload */,
          std::nullopt /* expected_origin_header */
      },
      {
          "POST" /* original_method */,
          "POST" /* new_method */,
          "https://other.example.com/redirected.php" /* new_url */,
          {{"Header1", "Value1"}, {"Header2", "Value2"}} /* modified_headers */,
          false /* expected_should_clear_upload */,
          "null" /* expected_origin_header */
      },
      {
          "POST" /* original_method */,
          "GET" /* new_method */,
          "https://other.example.com/redirected.php" /* new_url */,
          {{"Header1", "Value1"}, {"Header2", "Value2"}} /* modified_headers */,
          true /* expected_should_clear_upload */,
          std::nullopt /* expected_origin_header */
      },
      {
          "PUT" /* original_method */,
          "GET" /* new_method */,
          "https://www.example.com/redirected.php" /* new_url */,
          {{"Header1", "Value1"}, {"Header2", "Value2"}} /* modified_headers */,
          true /* expected_should_clear_upload */,
          std::nullopt /* expected_origin_header */
      },
      {
          "FOOT" /* original_method */,
          "GET" /* new_method */,
          "https://www.example.com/redirected.php" /* new_url */,
          {{"Header1", "Value1"}, {"Header2", "Value2"}} /* modified_headers */,
          true /* expected_should_clear_upload */,
          std::nullopt /* expected_origin_header */
      },
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(::testing::Message()
                 << "original_method: " << test.original_method
                 << " new_method: " << test.new_method
                 << " new_url: " << test.new_url);
    RedirectInfo redirect_info;
    redirect_info.new_method = test.new_method;
    redirect_info.new_url = GURL(test.new_url);

    net::HttpRequestHeaders modified_headers;
    for (const auto& headers : test.modified_headers) {
      ASSERT_TRUE(!!headers.name);  // Currently all test case has this.
      modified_headers.SetHeader(headers.name, headers.value);
    }
    std::optional<std::string> expected_modified_header1 =
        modified_headers.GetHeader("Header1");
    std::optional<std::string> expected_modified_header2 =
        modified_headers.GetHeader("Header2");

    HttpRequestHeaders request_headers;
    request_headers.SetHeader(HttpRequestHeaders::kOrigin,
                              "https://origin.example.com");
    request_headers.SetHeader(HttpRequestHeaders::kContentLength,
                              kContentLengthValue);
    request_headers.SetHeader(HttpRequestHeaders::kContentType,
                              kContentTypeValue);
    request_headers.SetHeader(kContentEncoding, kContentEncodingValue);
    request_headers.SetHeader(kContentLanguage, kContentLanguageValue);
    request_headers.SetHeader(kContentLocation, kContentLocationValue);
    request_headers.SetHeader(kCustomHeader, kCustomHeaderValue);
    request_headers.SetHeader("Header1", "Initial-Value1");

    bool should_clear_upload = !test.expected_should_clear_upload;

    RedirectUtil::UpdateHttpRequest(
        original_url, test.original_method, redirect_info,
        std::nullopt /* removed_headers */, modified_headers, &request_headers,
        &should_clear_upload);
    EXPECT_EQ(test.expected_should_clear_upload, should_clear_upload);
    std::optional<std::string> expected_content_length;
    std::optional<std::string> expected_content_type;
    std::optional<std::string> expected_content_encoding;
    std::optional<std::string> expected_content_language;
    std::optional<std::string> expected_content_location;
    if (!test.expected_should_clear_upload) {
      expected_content_length = kContentLengthValue;
      expected_content_type = kContentTypeValue;
      expected_content_encoding = kContentEncodingValue;
      expected_content_language = kContentLanguageValue;
      expected_content_location = kContentLocationValue;
    }

    EXPECT_EQ(request_headers.GetHeader(HttpRequestHeaders::kContentLength),
              expected_content_length);
    EXPECT_EQ(request_headers.GetHeader(HttpRequestHeaders::kContentType),
              expected_content_type);
    EXPECT_EQ(request_headers.GetHeader(kContentEncoding),
              expected_content_encoding);
    EXPECT_EQ(request_headers.GetHeader(kContentLanguage),
              expected_content_language);
    EXPECT_EQ(request_headers.GetHeader(kContentLocation),
              expected_content_location);

    EXPECT_EQ(kCustomHeaderValue, request_headers.GetHeader(kCustomHeader));

    EXPECT_EQ(request_headers.GetHeader(HttpRequestHeaders::kOrigin),
              test.expected_origin_header);

    EXPECT_EQ(expected_modified_header1, request_headers.GetHeader("Header1"));
    EXPECT_EQ(expected_modified_header2, request_headers.GetHeader("Header2"));
  }
}

TEST(RedirectUtilTest, RemovedHeaders) {
  struct TestCase {
    std::vector<const char*> initial_headers;
    std::vector<const char*> modified_headers;
    std::vector<const char*> removed_headers;
    std::vector<const char*> final_headers;
  };
  const TestCase kTests[] = {
      // Remove no headers (empty vector).
      {
          {},  // Initial headers
          {},  // Modified headers
          {},  // Removed headers
          {},  // Final headers
      },
      // Remove an existing header.
      {
          {"A:0"},  // Initial headers
          {},       // Modified headers
          {"A"},    // Removed headers
          {},       // Final headers
      },
      // Remove a missing header.
      {
          {},     // Initial headers
          {},     // Modified headers
          {"A"},  // Removed headers
          {},     // Final headers
      },
      // Remove two different headers.
      {
          {"A:0", "B:0"},  // Initial headers
          {},              // Modified headers
          {"A", "B"},      // Removed headers
          {},              // Final headers
      },
      // Remove two times the same headers.
      {
          {"A:0"},     // Initial headers
          {},          // Modified headers
          {"A", "A"},  // Removed headers
          {},          // Final headers
      },
      // Remove an existing header that is also modified.
      {
          {"A:0"},  // Initial headers
          {"A:1"},  // Modified headers
          {"A"},    // Removed headers
          {"A:1"},  // Final headers
      },
      // Some headers are removed, some aren't.
      {
          {"A:0", "B:0"},  // Initial headers
          {},              // Modified headers
          {"A"},           // Removed headers
          {"B:0"},         // Final headers
      },
  };

  for (const auto& test : kTests) {
    HttpRequestHeaders initial_headers, modified_headers, final_headers;
    std::vector<std::string> removed_headers;
    for (const char* header : test.initial_headers)
      initial_headers.AddHeaderFromString(header);
    for (const char* header : test.modified_headers)
      modified_headers.AddHeaderFromString(header);
    for (const char* header : test.removed_headers)
      removed_headers.push_back(header);
    for (const char* header : test.final_headers)
      final_headers.AddHeaderFromString(header);
    bool should_clear_upload(false);  // unused.

    RedirectUtil::UpdateHttpRequest(GURL(),         // original_url
                                    std::string(),  // original_method
                                    RedirectInfo(), removed_headers,
                                    modified_headers, &initial_headers,
                                    &should_clear_upload);

    // The initial_headers have been updated and should match the expected final
    // headers.
    EXPECT_EQ(initial_headers.ToString(), final_headers.ToString());
  }
}

// Test with removed_headers = std::nullopt.
TEST(RedirectUtilTest, RemovedHeadersNullOpt) {
  HttpRequestHeaders initial_headers, final_headers;
  initial_headers.SetHeader("A", "0");
  final_headers.SetHeader("A", "0");
  std::optional<std::vector<std::string>> removed_headers(std::nullopt);
  std::optional<HttpRequestHeaders> modified_headers(std::in_place);
  bool should_clear_upload(false);  // unused.

  RedirectUtil::UpdateHttpRequest(GURL(),         // original_url
                                  std::string(),  // original_method
                                  RedirectInfo(), removed_headers,
                                  modified_headers, &initial_headers,
                                  &should_clear_upload);

  // The initial_headers have been updated and should match the expected final
  // headers.
  EXPECT_EQ(initial_headers.ToString(), final_headers.ToString());
}

// Test with modified_headers = std::nullopt.
TEST(RedirectUtilTest, ModifyHeadersNullopt) {
  HttpRequestHeaders initial_headers, final_headers;
  initial_headers.SetHeader("A", "0");
  final_headers.SetHeader("A", "0");
  std::optional<std::vector<std::string>> removed_headers(std::in_place);
  std::optional<HttpRequestHeaders> modified_headers(std::nullopt);
  bool should_clear_upload(false);  // unused.

  RedirectUtil::UpdateHttpRequest(GURL(),         // original_url
                                  std::string(),  // original_method
                                  RedirectInfo(), removed_headers,
                                  modified_headers, &initial_headers,
                                  &should_clear_upload);

  // The initial_headers have been updated and should match the expected final
  // headers.
  EXPECT_EQ(initial_headers.ToString(), final_headers.ToString());
}

}  // namespace
}  // namespace net
