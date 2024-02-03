// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/web_package/web_package_request_matcher.h"

#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

constexpr char kVariantsHeader[] = "variants-04";
constexpr char kVariantKeyHeader[] = "variant-key-04";

TEST(WebPackageRequestMatcherTest, CacheBehavior) {
  const struct TestCase {
    const char* name;
    std::map<std::string, std::string> req_headers;
    std::vector<std::pair<std::string, std::vector<std::string>>> variants;
    std::vector<std::vector<std::string>> expected;
  } cases[] = {
      // Accept
      {"vanilla content-type",
       {{"accept", "text/html"}},
       {{"Accept", {"text/html"}}},
       {{"text/html"}}},
      {"client supports two content-types",
       {{"accept", "text/html, image/jpeg"}},
       {{"Accept", {"text/html"}}},
       {{"text/html"}}},
      {"format miss",
       {{"accept", "image/jpeg"}},
       {{"Accept", {"text/html"}}},
       {{"text/html"}}},
      {"no format preference",
       {},
       {{"Accept", {"text/html"}}},
       {{"text/html"}}},
      {"no available format",
       {{"accept", "text/html"}},
       {{"Accept", {}}},
       {{}}},
      {"accept all types",
       {{"accept", "*/*"}},
       {{"Accept", {"text/html", "image/jpeg"}}},
       {{"text/html", "image/jpeg"}}},
      {"accept all subtypes",
       {{"accept", "image/*"}},
       {{"Accept", {"text/html", "image/jpeg"}}},
       {{"image/jpeg"}}},
      {"type params match",
       {{"accept", "text/html;param=bar"}},
       {{"Accept", {"text/html;param=foo", "text/html;param=bar"}}},
       {{"text/html;param=bar"}}},
      {"type with q value",
       {{"accept", "text/html;q=0.8;param=foo"}},
       {{"Accept", {"image/jpeg", "text/html;param=foo"}}},
       {{"text/html;param=foo"}}},
      {"type with zero q value",
       {{"accept", "text/html;q=0.0, image/jpeg"}},
       {{"Accept", {"text/html", "image/jpeg"}}},
       {{"image/jpeg"}}},
      {"type with invalid q value",
       {{"accept", "text/html;q=999, image/jpeg"}},
       {{"Accept", {"text/html", "image/jpeg"}}},
       {{"text/html", "image/jpeg"}}},
      // Accept-Encoding
      {"vanilla encoding",
       {{"accept-encoding", "gzip"}},
       {{"Accept-Encoding", {"gzip"}}},
       {{"gzip", "identity"}}},
      {"client supports two encodings",
       {{"accept-encoding", "gzip, br"}},
       {{"Accept-Encoding", {"gzip"}}},
       {{"gzip", "identity"}}},
      {"two stored, two preferences",
       {{"accept-encoding", "gzip, br"}},
       {{"Accept-Encoding", {"gzip", "br"}}},
       {{"gzip", "br", "identity"}}},
      {"no encoding preference",
       {},
       {{"Accept-Encoding", {"gzip"}}},
       {{"identity"}}},
      // Accept-Language
      {"vanilla language",
       {{"accept-language", "en"}},
       {{"Accept-Language", {"en"}}},
       {{"en"}}},
      {"multiple languages",
       {{"accept-language", "en, JA"}},
       {{"Accept-Language", {"en", "fr", "ja"}}},
       {{"en", "ja"}}},
      {"no language preference",
       {},
       {{"Accept-Language", {"en", "ja"}}},
       {{"en"}}},
      {"no available language",
       {{"accept-language", "en"}},
       {{"Accept-Language", {}}},
       {{}}},
      {"accept all languages",
       {{"accept-language", "*"}},
       {{"Accept-Language", {"en", "ja"}}},
       {{"en", "ja"}}},
      {"language subtag",
       {{"accept-language", "en"}},
       {{"Accept-Language", {"en-US", "enq"}}},
       {{"en-US"}}},
      {"language with q values",
       {{"accept-language", "ja, en;q=0.8"}},
       {{"Accept-Language", {"fr", "en", "ja"}}},
       {{"ja", "en"}}},
      {"language with zero q value",
       {{"accept-language", "ja, en;q=0"}},
       {{"Accept-Language", {"fr", "en"}}},
       {{"fr"}}},
      // Multiple axis
      {"format and language matches",
       {{"accept", "text/html"}, {"accept-language", "en"}},
       {{"Accept", {"text/html"}}, {"Accept-Language", {"en", "fr"}}},
       {{"text/html"}, {"en"}}},
      {"accept anything",
       {{"accept", "*/*"}, {"accept-language", "*"}},
       {{"Accept", {"text/html", "image/jpeg"}},
        {"Accept-Language", {"en", "fr"}}},
       {{"text/html", "image/jpeg"}, {"en", "fr"}}},
      {"unknown field name",
       {{"accept-language", "en"}, {"unknown", "foo"}},
       {{"Accept-Language", {"en"}}, {"Unknown", {"foo"}}},
       {{"en"}}},
  };
  for (const auto& c : cases) {
    net::HttpRequestHeaders request_headers;
    for (auto it = c.req_headers.begin(); it != c.req_headers.end(); ++it)
      request_headers.SetHeader(it->first, it->second);
    EXPECT_EQ(c.expected, WebPackageRequestMatcher::CacheBehavior(
                              c.variants, request_headers))
        << c.name;
  }
}

TEST(WebPackageRequestMatcherTest, MatchRequest) {
  const struct TestCase {
    const char* name;
    std::map<std::string, std::string> req_headers;
    WebPackageRequestMatcher::HeaderMap res_headers;
    bool should_match;
  } cases[] = {
      {"no variants and variant-key", {{"accept", "text/html"}}, {}, true},
      {"has variants but no variant-key",
       {{"accept", "text/html"}},
       {{kVariantsHeader, "Accept; text/html"}},
       false},
      {"has variant-key but no variants",
       {{"accept", "text/html"}},
       {{kVariantKeyHeader, "text/html"}},
       false},
      {"content type matches",
       {{"accept", "text/html"}},
       {{kVariantsHeader, "Accept; text/html; image/jpeg"},
        {kVariantKeyHeader, "text/html"}},
       true},
      {"content type misses",
       {{"accept", "image/jpeg"}},
       {{kVariantsHeader, "Accept; text/html; image/jpeg"},
        {kVariantKeyHeader, "text/html"}},
       false},
      {"encoding matches",
       {},
       {{kVariantsHeader, "Accept-Encoding;gzip;identity"},
        {kVariantKeyHeader, "identity"}},
       true},
      {"encoding misses",
       {},
       {{kVariantsHeader, "Accept-Encoding;gzip;identity"},
        {kVariantKeyHeader, "gzip"}},
       false},
      {"language matches",
       {{"accept-language", "en"}},
       {{kVariantsHeader, "Accept-Language;en;ja"}, {kVariantKeyHeader, "en"}},
       true},
      {"language misses",
       {{"accept-language", "ja"}},
       {{kVariantsHeader, "Accept-Language;en;ja"}, {kVariantKeyHeader, "en"}},
       false},
      {"content type and language match",
       {{"accept", "text/html"}, {"accept-language", "en"}},
       {{kVariantsHeader, "Accept-Language;fr;en, Accept;text/plain;text/html"},
        {kVariantKeyHeader, "en;text/html"}},
       true},
      {"content type matches but language misses",
       {{"accept", "text/html"}, {"accept-language", "fr"}},
       {{kVariantsHeader, "Accept-Language;fr;en, Accept;text/plain;text/html"},
        {kVariantKeyHeader, "en;text/html"}},
       false},
      {"language matches but content type misses",
       {{"accept", "text/plain"}, {"accept-language", "en"}},
       {{kVariantsHeader, "Accept-Language;fr;en, Accept;text/plain;text/html"},
        {kVariantKeyHeader, "en;text/html"}},
       false},
      {"multiple variant key",
       {{"accept-encoding", "identity"}, {"accept-language", "fr"}},
       {{kVariantsHeader, "Accept-Encoding;gzip;br, Accept-Language;en;fr"},
        {kVariantKeyHeader, "gzip;fr, identity;fr"}},
       true},
      {"bad variant key item length",
       {},
       {{kVariantsHeader, "Accept;text/html, Accept-Language;en;fr"},
        {kVariantKeyHeader, "text/html;en, text/html;fr;oops"}},
       false},
      {"unknown field name",
       {{"accept-language", "en"}, {"unknown", "foo"}},
       {{kVariantsHeader, "Accept-Language;en, Unknown;foo"},
        {kVariantKeyHeader, "en;foo"}},
       false},
  };
  for (const auto& c : cases) {
    net::HttpRequestHeaders request_headers;
    for (auto it = c.req_headers.begin(); it != c.req_headers.end(); ++it)
      request_headers.SetHeader(it->first, it->second);
    EXPECT_EQ(c.should_match, WebPackageRequestMatcher::MatchRequest(
                                  request_headers, c.res_headers))
        << c.name;
  }
}

TEST(WebPackageRequestMatcherTest, FindBestMatchingVariantKey) {
  const struct TestCase {
    const char* name;
    std::map<std::string, std::string> req_headers;
    std::string variants;
    std::vector<std::string> variant_key_list;
    std::optional<std::string> expected_result;
  } cases[] = {
      {
          "Content type negotiation: default value",
          {{"accept", "image/webp,image/jpg"}},
          "Accept;image/xx;image/yy",
          {"image/yy", "image/xx"},
          "image/xx"  // There is no preferred available, image/xx is the
                      // default.
      },
      {
          "Language negotiation: default value",
          {{"accept-language", "en,fr"}},
          "accept-language;ja;ch",
          {"ja", "ch"},
          "ja"  // There is no preferred available, ja is the default.
      },
      {
          "Language negotiation: no matching language",
          {{"accept-language", "en,fr"}},
          "accept-language;ja;ch",
          {"ch"},
          std::nullopt  // There is no matching language.
      },
      {
          "Content type negotiation: Q value",
          {{"accept", "image/jpg;q=0.8,image/webp,image/apng"}},
          "Accept;image/jpg;image/apng;image/webp",
          {"image/jpg", "image/webp", "image/apng"},
          "image/webp"  // image/webp is the most preferred content type.
      },
      {
          "Content type and Language negotiation",
          {{"accept", "image/webp,image/jpg,*/*;q=0.3"},
           {"accept-language", "en,fr,ja"}},
          "Accept;image/webp;image/apng,accept-language;ja;en",
          {"image/apng;ja", "image/webp;ja", "image/apng;en,image/webp;en"},
          "image/apng;en,image/webp;en"  // image/webp;en is the most preferred
                                         // content type.
      },
      {
          "Variants is invalid",
          {{"accept", "image/webp,image/jpg"}},
          " ",
          {},
          std::nullopt  // Variants is invalid.
      },
      {
          "Variants size and Variant Key size don't match",
          {{"accept", "image/webp,image/jpg"}},
          "Accept;image/webp;image/apng,accept-language;ja;en",
          {"image/webp", "image/apng"},
          std::nullopt  // There is no matching Variant Key.
      },
      {
          "Unknown Variant",
          {{"accept", "image/webp,image/jpg"}},
          "Accept;image/webp;image/jpg, FooBar;foo;bar",
          {"image/webp;foo", "image/webp;bar", "image/webp;jpg",
           "image/jpg;bar"},
          std::nullopt  // FooBar is unknown.
      },
  };
  for (const auto& c : cases) {
    net::HttpRequestHeaders request_headers;
    for (auto it = c.req_headers.begin(); it != c.req_headers.end(); ++it)
      request_headers.SetHeader(it->first, it->second);
    auto variant_key_list_it =
        WebPackageRequestMatcher::FindBestMatchingVariantKey(
            request_headers, c.variants, c.variant_key_list);
    if (variant_key_list_it == c.variant_key_list.end()) {
      EXPECT_EQ(c.expected_result, std::nullopt) << c.name;
    } else {
      EXPECT_EQ(c.expected_result, *variant_key_list_it) << c.name;
    }
  }
}

TEST(WebPackageRequestMatcherTest, FindBestMatchingIndex) {
  const struct TestCase {
    const char* name;
    std::string variants;
    std::map<std::string, std::string> req_headers;
    std::optional<size_t> expected_result;
  } cases[] = {
      {"matching value",
       "Accept;image/png;image/jpg",
       {{"accept", "image/webp,image/jpg"}},
       1 /* image/jpg */},
      {"default value",
       "Accept;image/xx;image/yy",
       {{"accept", "image/webp,image/jpg"}},
       0 /* image/xx */},
      {"content type and language",
       "Accept;image/png;image/jpg, Accept-Language;en;fr;ja",
       {{"accept", "image/jpg"}, {"accept-language", "fr"}},
       4 /* image/jpg, fr */},
      {"language and content type",
       "Accept-Language;en;fr;ja, Accept;image/png;image/jpg",
       {{"accept", "image/jpg"}, {"accept-language", "fr"}},
       3 /* fr, image/jpg */},
      {"ill-formed variants",
       "Accept",
       {{"accept", "image/webp,image/jpg"}},
       std::nullopt},
      {"unknown field name",
       "Unknown;foo;bar",
       {{"Unknown", "foo"}},
       std::nullopt},
  };

  for (const auto& c : cases) {
    net::HttpRequestHeaders request_headers;
    for (auto it = c.req_headers.begin(); it != c.req_headers.end(); ++it)
      request_headers.SetHeader(it->first, it->second);
    std::optional<size_t> result =
        WebPackageRequestMatcher::FindBestMatchingIndex(request_headers,
                                                        c.variants);
    EXPECT_EQ(c.expected_result, result) << c.name;
  }
}

}  // namespace blink
