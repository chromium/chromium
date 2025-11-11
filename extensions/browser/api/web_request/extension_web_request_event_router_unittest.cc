// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/extension_web_request_event_router.h"

#include "base/test/values_test_util.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_resource_type.h"
#include "extensions/common/url_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

using RequestFilter = WebRequestEventRouter::RequestFilter;

bool AreFiltersEqual(const RequestFilter& f1, const RequestFilter& f2) {
  if (f1.tab_id != f2.tab_id || f1.window_id != f2.window_id) {
    return false;
  }
  if (f1.types != f2.types) {
    return false;
  }
  return f1.urls.patterns() == f2.urls.patterns();
}

class WebRequestEventRouterTest : public testing::Test {
 protected:
  // Helper to easily create a `URLPattern`.
  URLPattern CreatePattern(const std::string& pattern_str) {
    URLPattern pattern(kWebRequestFilterValidSchemes);
    EXPECT_EQ(URLPattern::ParseResult::kSuccess, pattern.Parse(pattern_str))
        << "Failed to parse pattern: " << pattern_str;
    return pattern;
  }

  // Helper to test the serialization and deserialization round trip.
  void TestRoundTrip(const RequestFilter& original) {
    base::Value::Dict serialized = original.ToValue();

    RequestFilter deserialized;
    std::string error;
    ASSERT_TRUE(deserialized.InitFromValue(serialized, &error))
        << "Deserialization failed with error: " << error;
    EXPECT_TRUE(error.empty());

    EXPECT_TRUE(AreFiltersEqual(original, deserialized));
  }
};

}  // namespace

// Test serialization and deserialization of an empty filter.
TEST_F(WebRequestEventRouterTest, RequestFilter_Empty) {
  RequestFilter filter;

  // An empty filter should serialize to {"urls": []} because `ToValue()`
  // ensures the "urls" key is present.
  base::Value::Dict expected = base::test::ParseJsonDict(R"({"urls": []})");
  EXPECT_EQ(expected, filter.ToValue());

  TestRoundTrip(filter);
}

// Test serialization and deserialization with only URLs.
TEST_F(WebRequestEventRouterTest, RequestFilter_UrlsOnly) {
  RequestFilter filter;
  filter.urls.AddPattern(CreatePattern("http://example.com/foo*"));
  filter.urls.AddPattern(CreatePattern("*://www.google.com/*"));

  // The order in the serialized list depends on the internal ordering
  // of `URLPattern` (lexicographical sort of the pattern string).
  base::Value::Dict expected = base::test::ParseJsonDict(R"({
    "urls": [
      "*://www.google.com/*",
      "http://example.com/foo*"
    ]
  })");

  EXPECT_EQ(expected, filter.ToValue());
  TestRoundTrip(filter);
}

// Test serialization and deserialization with resource types.
TEST_F(WebRequestEventRouterTest, RequestFilter_Types) {
  RequestFilter filter;
  filter.types.push_back(WebRequestResourceType::SCRIPT);
  filter.types.push_back(WebRequestResourceType::XHR);
  filter.types.push_back(WebRequestResourceType::MAIN_FRAME);
  filter.types.push_back(WebRequestResourceType::WEB_SOCKET);

  // NOTE: "urls": [] is always added by `ToValue()`.
  base::Value::Dict expected = base::test::ParseJsonDict(R"({
    "urls": [],
    "types": ["script", "xmlhttprequest", "main_frame", "websocket"]
  })");

  EXPECT_EQ(expected, filter.ToValue());
  TestRoundTrip(filter);
}

// Test serialization and deserialization with IDs.
TEST_F(WebRequestEventRouterTest, RequestFilter_Ids) {
  RequestFilter filter;
  filter.tab_id = 42;
  filter.window_id = 101;

  // NOTE: "urls": [] is always added by `ToValue()`.
  base::Value::Dict expected = base::test::ParseJsonDict(R"({
    "urls": [],
    "tabId": 42,
    "windowId": 101
  })");

  EXPECT_EQ(expected, filter.ToValue());
  TestRoundTrip(filter);
}

// Test a fully populated filter.
TEST_F(WebRequestEventRouterTest, RequestFilter_Full) {
  RequestFilter filter;
  filter.urls.AddPattern(CreatePattern("https://www.google.com/*"));
  filter.types.push_back(WebRequestResourceType::IMAGE);
  filter.types.push_back(WebRequestResourceType::STYLESHEET);
  filter.tab_id = 42;
  filter.window_id = 101;

  base::Value::Dict expected = base::test::ParseJsonDict(R"({
    "urls": ["https://www.google.com/*"],
    "types": ["image", "stylesheet"],
    "tabId": 42,
    "windowId": 101
  })");

  EXPECT_EQ(expected, filter.ToValue());
  TestRoundTrip(filter);
}

// Test deserialization failure due to an invalid URL pattern.
TEST_F(WebRequestEventRouterTest, RequestFilter_DeserializeInvalidUrl) {
  base::Value::Dict input = base::test::ParseJsonDict(R"({
    "urls": ["http://example.com/*", ":::invalid:::"]
  })");

  RequestFilter filter;
  std::string error;
  EXPECT_FALSE(filter.InitFromValue(input, &error));
  EXPECT_EQ("':::invalid:::' is not a valid URL pattern.", error);
}

// Test deserialization failure due to an invalid resource type string.
TEST_F(WebRequestEventRouterTest, RequestFilter_DeserializeInvalidType) {
  base::Value::Dict input = base::test::ParseJsonDict(R"({
    "urls": [],
    "types": ["script", "bad_type"]
  })");

  RequestFilter filter;
  std::string error;
  EXPECT_FALSE(filter.InitFromValue(input, &error));
}

// Test deserialization failure due to incorrect data types for fields.
TEST_F(WebRequestEventRouterTest, RequestFilter_DeserializeWrongDataType) {
  RequestFilter filter;
  std::string error;

  // Case 1: "urls" is not a list.
  base::Value::Dict input_urls =
      base::test::ParseJsonDict(R"({"urls": "string"})");
  EXPECT_FALSE(filter.InitFromValue(input_urls, &error));

  // Case 2: "types" is not a list.
  base::Value::Dict input_types =
      base::test::ParseJsonDict(R"({"urls":[], "types": 123})");
  EXPECT_FALSE(filter.InitFromValue(input_types, &error));

  // Case 3: "tabId" is not an int.
  base::Value::Dict input_tabid =
      base::test::ParseJsonDict(R"({"urls":[], "tabId": "string"})");
  EXPECT_FALSE(filter.InitFromValue(input_tabid, &error));

  // Case 4: "windowId" is not an int.
  base::Value::Dict input_windowid =
      base::test::ParseJsonDict(R"({"urls":[], "windowId": "string"})");
  EXPECT_FALSE(filter.InitFromValue(input_windowid, &error));
}

// Test deserialization behavior when "urls" is missing.
TEST_F(WebRequestEventRouterTest, RequestFilter_DeserializeMissingUrls) {
  RequestFilter filter;
  std::string error;

  // Case 1: Empty dictionary.
  base::Value::Dict input_empty = base::test::ParseJsonDict(R"({})");
  EXPECT_FALSE(filter.InitFromValue(input_empty, &error));

  // Case 2: Dictionary with other keys but missing "urls".
  base::Value::Dict input_missing =
      base::test::ParseJsonDict(R"({"types": ["script"]})");
  RequestFilter filter_missing;
  EXPECT_FALSE(filter_missing.InitFromValue(input_missing, &error));
}

}  // namespace extensions
