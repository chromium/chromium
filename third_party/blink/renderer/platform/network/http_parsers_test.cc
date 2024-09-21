// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/network/http_parsers.h"

#include <string_view>

#include "base/containers/span.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "services/network/public/mojom/content_security_policy.mojom-blink-forward.h"
#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "services/network/public/mojom/parsed_headers.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

TEST(HTTPParsersTest, ParseCacheControl) {
  CacheControlHeader header;

  header =
      ParseCacheControlDirectives(AtomicString("no-cache"), AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_TRUE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_EQ(std::nullopt, header.max_age);
  EXPECT_EQ(std::nullopt, header.stale_while_revalidate);

  header = ParseCacheControlDirectives(AtomicString("no-cache no-store"),
                                       AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_TRUE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_EQ(std::nullopt, header.max_age);
  EXPECT_EQ(std::nullopt, header.stale_while_revalidate);

  header = ParseCacheControlDirectives(AtomicString("no-store must-revalidate"),
                                       AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_FALSE(header.contains_no_cache);
  EXPECT_TRUE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_EQ(std::nullopt, header.max_age);
  EXPECT_EQ(std::nullopt, header.stale_while_revalidate);

  header =
      ParseCacheControlDirectives(AtomicString("max-age=0"), AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_FALSE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_EQ(base::TimeDelta(), header.max_age.value());
  EXPECT_EQ(std::nullopt, header.stale_while_revalidate);

  header = ParseCacheControlDirectives(AtomicString("max-age"), AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_FALSE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_EQ(std::nullopt, header.max_age);
  EXPECT_EQ(std::nullopt, header.stale_while_revalidate);

  header = ParseCacheControlDirectives(AtomicString("max-age=0 no-cache"),
                                       AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_FALSE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_EQ(base::TimeDelta(), header.max_age.value());
  EXPECT_EQ(std::nullopt, header.stale_while_revalidate);

  header =
      ParseCacheControlDirectives(AtomicString("no-cache=foo"), AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_FALSE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_EQ(std::nullopt, header.max_age);
  EXPECT_EQ(std::nullopt, header.stale_while_revalidate);

  header =
      ParseCacheControlDirectives(AtomicString("nonsense"), AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_FALSE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_EQ(std::nullopt, header.max_age);
  EXPECT_EQ(std::nullopt, header.stale_while_revalidate);

  header = ParseCacheControlDirectives(AtomicString("\rno-cache\n\t\v\0\b"),
                                       AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_TRUE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_EQ(std::nullopt, header.max_age);
  EXPECT_EQ(std::nullopt, header.stale_while_revalidate);

  header = ParseCacheControlDirectives(AtomicString("      no-cache       "),
                                       AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_TRUE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_EQ(std::nullopt, header.max_age);
  EXPECT_EQ(std::nullopt, header.stale_while_revalidate);

  header =
      ParseCacheControlDirectives(AtomicString(), AtomicString("no-cache"));
  EXPECT_TRUE(header.parsed);
  EXPECT_TRUE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_EQ(std::nullopt, header.max_age);
  EXPECT_EQ(std::nullopt, header.stale_while_revalidate);

  header = ParseCacheControlDirectives(
      AtomicString("stale-while-revalidate=2,stale-while-revalidate=3"),
      AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_FALSE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_EQ(std::nullopt, header.max_age);
  EXPECT_EQ(2.0, header.stale_while_revalidate.value().InSecondsF());
}

TEST(HTTPParsersTest, CommaDelimitedHeaderSet) {
  CommaDelimitedHeaderSet set1;
  CommaDelimitedHeaderSet set2;
  ParseCommaDelimitedHeader("dpr, rw, whatever", set1);
  EXPECT_TRUE(set1.Contains("dpr"));
  EXPECT_TRUE(set1.Contains("rw"));
  EXPECT_TRUE(set1.Contains("whatever"));
  ParseCommaDelimitedHeader("dprw\t     , fo\to", set2);
  EXPECT_FALSE(set2.Contains("dpr"));
  EXPECT_FALSE(set2.Contains("rw"));
  EXPECT_FALSE(set2.Contains("whatever"));
  EXPECT_TRUE(set2.Contains("dprw"));
  EXPECT_FALSE(set2.Contains("foo"));
  EXPECT_TRUE(set2.Contains("fo\to"));
}

TEST(HTTPParsersTest, HTTPToken) {
  const UChar kHiraganaA[2] = {0x3042, 0};
  const UChar kLatinCapitalAWithMacron[2] = {0x100, 0};

  EXPECT_TRUE(blink::IsValidHTTPToken("gzip"));
  EXPECT_TRUE(blink::IsValidHTTPToken("no-cache"));
  EXPECT_TRUE(blink::IsValidHTTPToken("86400"));
  EXPECT_TRUE(blink::IsValidHTTPToken("~"));
  EXPECT_FALSE(blink::IsValidHTTPToken(""));
  EXPECT_FALSE(blink::IsValidHTTPToken(" "));
  EXPECT_FALSE(blink::IsValidHTTPToken("\t"));
  EXPECT_FALSE(blink::IsValidHTTPToken("\x7f"));
  EXPECT_FALSE(blink::IsValidHTTPToken("\xff"));
  EXPECT_FALSE(blink::IsValidHTTPToken(String(kLatinCapitalAWithMacron)));
  EXPECT_FALSE(blink::IsValidHTTPToken("t a"));
  EXPECT_FALSE(blink::IsValidHTTPToken("()"));
  EXPECT_FALSE(blink::IsValidHTTPToken("(foobar)"));
  EXPECT_FALSE(blink::IsValidHTTPToken(String("\0", 1u)));
  EXPECT_FALSE(blink::IsValidHTTPToken(String(kHiraganaA)));
}

TEST(HTTPParsersTest, ExtractMIMETypeFromMediaType) {
  const AtomicString text_html("text/html");

  EXPECT_EQ(text_html, ExtractMIMETypeFromMediaType(AtomicString("text/html")));
  EXPECT_EQ(text_html, ExtractMIMETypeFromMediaType(
                           AtomicString("text/html; charset=iso-8859-1")));

  // Quoted charset parameter
  EXPECT_EQ(text_html, ExtractMIMETypeFromMediaType(
                           AtomicString("text/html; charset=\"quoted\"")));

  // Multiple parameters
  EXPECT_EQ(text_html, ExtractMIMETypeFromMediaType(
                           AtomicString("text/html; charset=x; foo=bar")));

  // OWSes are trimmed.
  EXPECT_EQ(text_html,
            ExtractMIMETypeFromMediaType(AtomicString(" text/html   ")));
  EXPECT_EQ(text_html,
            ExtractMIMETypeFromMediaType(AtomicString("\ttext/html \t")));
  EXPECT_EQ(text_html, ExtractMIMETypeFromMediaType(
                           AtomicString("text/html ; charset=iso-8859-1")));

  // Non-standard multiple type/subtype listing using a comma as a separator
  // is accepted.
  EXPECT_EQ(text_html,
            ExtractMIMETypeFromMediaType(AtomicString("text/html,text/plain")));
  EXPECT_EQ(text_html, ExtractMIMETypeFromMediaType(
                           AtomicString("text/html , text/plain")));
  EXPECT_EQ(text_html, ExtractMIMETypeFromMediaType(
                           AtomicString("text/html\t,\ttext/plain")));
  EXPECT_EQ(text_html, ExtractMIMETypeFromMediaType(AtomicString(
                           "text/html,text/plain;charset=iso-8859-1")));

  // Preserves case.
  EXPECT_EQ("tExt/hTMl",
            ExtractMIMETypeFromMediaType(AtomicString("tExt/hTMl")));

  EXPECT_EQ(g_empty_string,
            ExtractMIMETypeFromMediaType(AtomicString(", text/html")));
  EXPECT_EQ(g_empty_string,
            ExtractMIMETypeFromMediaType(AtomicString("; text/html")));

  // If no normalization is required, the same AtomicString should be returned.
  const AtomicString& passthrough = ExtractMIMETypeFromMediaType(text_html);
  EXPECT_EQ(text_html.Impl(), passthrough.Impl());
}

TEST(HTTPParsersTest, MinimizedMIMEType) {
  EXPECT_EQ("text/javascript",
            MinimizedMIMEType(AtomicString("application/javascript")));
  EXPECT_EQ("application/json", MinimizedMIMEType(AtomicString("text/json")));
  EXPECT_EQ("image/svg+xml", MinimizedMIMEType(AtomicString("image/svg+xml")));
  EXPECT_EQ("application/xml",
            MinimizedMIMEType(AtomicString("application/rss+xml")));
  EXPECT_EQ("image/png", MinimizedMIMEType(AtomicString("image/png")));
}

TEST(HTTPParsersTest, ExtractMIMETypeFromMediaTypeInvalidInput) {
  // extractMIMETypeFromMediaType() returns the string before the first
  // semicolon after trimming OWSes at the head and the tail even if the
  // string doesn't conform to the media-type ABNF defined in the RFC 7231.

  // These behaviors could be fixed later when ready.

  // Non-OWS characters meaning space are not trimmed.
  EXPECT_EQ(AtomicString("\r\ntext/html\r\n"),
            ExtractMIMETypeFromMediaType(AtomicString("\r\ntext/html\r\n")));
  // U+2003, EM SPACE (UTF-8: E2 80 83).
  EXPECT_EQ(AtomicString::FromUTF8("\xE2\x80\x83text/html"),
            ExtractMIMETypeFromMediaType(
                AtomicString::FromUTF8("\xE2\x80\x83text/html")));

  // Invalid type/subtype.
  EXPECT_EQ(AtomicString("a"), ExtractMIMETypeFromMediaType(AtomicString("a")));

  // Invalid parameters.
  EXPECT_EQ(AtomicString("text/html"),
            ExtractMIMETypeFromMediaType(AtomicString("text/html;wow")));
  EXPECT_EQ(AtomicString("text/html"),
            ExtractMIMETypeFromMediaType(AtomicString("text/html;;;;;;")));
  EXPECT_EQ(AtomicString("text/html"),
            ExtractMIMETypeFromMediaType(AtomicString("text/html; = = = ")));

  // Only OWSes at either the beginning or the end of the type/subtype
  // portion.
  EXPECT_EQ(AtomicString("text / html"),
            ExtractMIMETypeFromMediaType(AtomicString("text / html")));
  EXPECT_EQ(AtomicString("t e x t / h t m l"),
            ExtractMIMETypeFromMediaType(AtomicString("t e x t / h t m l")));

  EXPECT_EQ(AtomicString("text\r\n/\nhtml"),
            ExtractMIMETypeFromMediaType(AtomicString("text\r\n/\nhtml")));
  EXPECT_EQ(AtomicString("text\n/\nhtml"),
            ExtractMIMETypeFromMediaType(AtomicString("text\n/\nhtml")));
  EXPECT_EQ(AtomicString::FromUTF8("text\xE2\x80\x83/html"),
            ExtractMIMETypeFromMediaType(
                AtomicString::FromUTF8("text\xE2\x80\x83/html")));
}

TEST(HTTPParsersTest, ParseHTTPRefresh) {
  base::TimeDelta delay;
  String url;
  EXPECT_FALSE(ParseHTTPRefresh("", nullptr, delay, url));
  EXPECT_FALSE(ParseHTTPRefresh(" ", nullptr, delay, url));
  EXPECT_FALSE(ParseHTTPRefresh("1.3xyz url=foo", nullptr, delay, url));
  EXPECT_FALSE(ParseHTTPRefresh("1.3.4xyz url=foo", nullptr, delay, url));
  EXPECT_FALSE(ParseHTTPRefresh("1e1 url=foo", nullptr, delay, url));

  EXPECT_TRUE(ParseHTTPRefresh("123 ", nullptr, delay, url));
  EXPECT_EQ(base::Seconds(123), delay);
  EXPECT_TRUE(url.empty());

  EXPECT_TRUE(ParseHTTPRefresh("1 ; url=dest", nullptr, delay, url));
  EXPECT_EQ(base::Seconds(1), delay);
  EXPECT_EQ("dest", url);
  EXPECT_TRUE(
      ParseHTTPRefresh("1 ;\nurl=dest", IsASCIISpace<UChar>, delay, url));
  EXPECT_EQ(base::Seconds(1), delay);
  EXPECT_EQ("dest", url);
  EXPECT_TRUE(ParseHTTPRefresh("1 ;\nurl=dest", nullptr, delay, url));
  EXPECT_EQ(base::Seconds(1), delay);
  EXPECT_EQ("url=dest", url);

  EXPECT_TRUE(ParseHTTPRefresh("1 url=dest", nullptr, delay, url));
  EXPECT_EQ(base::Seconds(1), delay);
  EXPECT_EQ("dest", url);

  EXPECT_TRUE(
      ParseHTTPRefresh("10\nurl=dest", IsASCIISpace<UChar>, delay, url));
  EXPECT_EQ(base::Seconds(10), delay);
  EXPECT_EQ("dest", url);

  EXPECT_TRUE(
      ParseHTTPRefresh("1.5; url=dest", IsASCIISpace<UChar>, delay, url));
  EXPECT_EQ(base::Seconds(1), delay);
  EXPECT_EQ("dest", url);
  EXPECT_TRUE(
      ParseHTTPRefresh("1.5.9; url=dest", IsASCIISpace<UChar>, delay, url));
  EXPECT_EQ(base::Seconds(1), delay);
  EXPECT_EQ("dest", url);
  EXPECT_TRUE(
      ParseHTTPRefresh("7..; url=dest", IsASCIISpace<UChar>, delay, url));
  EXPECT_EQ(base::Seconds(7), delay);
  EXPECT_EQ("dest", url);
}

TEST(HTTPParsersTest, ParseMultipartHeadersResult) {
  struct {
    const std::string_view data;
    const bool result;
    const size_t end;
  } tests[] = {
      {"This is junk", false, 0},
      {"Foo: bar\nBaz:\n\nAfter:\n", true, 15},
      {"Foo: bar\nBaz:\n", false, 0},
      {"Foo: bar\r\nBaz:\r\n\r\nAfter:\r\n", true, 18},
      {"Foo: bar\r\nBaz:\r\n", false, 0},
      {"Foo: bar\nBaz:\r\n\r\nAfter:\n\n", true, 17},
      {"Foo: bar\r\nBaz:\n", false, 0},
      {"\r\n", true, 2},
  };
  for (size_t i = 0; i < std::size(tests); ++i) {
    ResourceResponse response;
    wtf_size_t end = 0;
    bool result = ParseMultipartHeadersFromBody(
        base::as_byte_span(tests[i].data), &response, &end);
    EXPECT_EQ(tests[i].result, result);
    EXPECT_EQ(tests[i].end, end);
  }
}

TEST(HTTPParsersTest, ParseMultipartHeaders) {
  ResourceResponse response;
  response.AddHttpHeaderField(AtomicString("foo"), AtomicString("bar"));
  response.AddHttpHeaderField(http_names::kLowerRange, AtomicString("piyo"));
  response.AddHttpHeaderField(http_names::kLowerContentLength,
                              AtomicString("999"));
  response.AddHttpHeaderField(http_names::kLowerSetCookie, AtomicString("a=1"));

  const char kData[] =
      "content-type: image/png\n"
      "content-length: 10\n"
      "set-cookie: x=2\n"
      "set-cookie: y=3\n"
      "\n";
  wtf_size_t end = 0;
  bool result = ParseMultipartHeadersFromBody(
      base::byte_span_from_cstring(kData), &response, &end);

  EXPECT_TRUE(result);
  EXPECT_EQ(strlen(kData), end);
  EXPECT_EQ("image/png",
            response.HttpHeaderField(http_names::kLowerContentType));
  EXPECT_EQ("10", response.HttpHeaderField(http_names::kLowerContentLength));
  EXPECT_EQ("bar", response.HttpHeaderField(AtomicString("foo")));
  EXPECT_EQ(AtomicString(), response.HttpHeaderField(http_names::kLowerRange));
  EXPECT_EQ("x=2, y=3", response.HttpHeaderField(http_names::kLowerSetCookie));
}

TEST(HTTPParsersTest, ParseMultipartHeadersContentCharset) {
  ResourceResponse response;
  const char kData[] = "content-type: text/html; charset=utf-8\n\n";
  wtf_size_t end = 0;
  bool result = ParseMultipartHeadersFromBody(
      base::byte_span_from_cstring(kData), &response, &end);

  EXPECT_TRUE(result);
  EXPECT_EQ(strlen(kData), end);
  EXPECT_EQ("text/html; charset=utf-8",
            response.HttpHeaderField(http_names::kLowerContentType));
  EXPECT_EQ("utf-8", response.TextEncodingName());
}

void testServerTimingHeader(const char* headerValue,
                            Vector<Vector<String>> expectedResults) {
  std::unique_ptr<ServerTimingHeaderVector> results =
      ParseServerTimingHeader(headerValue);
  EXPECT_EQ((*results).size(), expectedResults.size());
  unsigned i = 0;
  for (const auto& header : *results) {
    Vector<String> expectedResult = expectedResults[i++];
    EXPECT_EQ(header->Name(), expectedResult[0]);
    EXPECT_EQ(header->Duration(), expectedResult[1].ToDouble());
    EXPECT_EQ(header->Description(), expectedResult[2]);
  }
}

TEST(HTTPParsersTest, ParseServerTimingHeader) {
  // empty string
  testServerTimingHeader("", {});

  // name only
  testServerTimingHeader("metric", {{"metric", "0", ""}});

  // name and duration
  testServerTimingHeader("metric;dur=123.4", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric;dur=\"123.4\"", {{"metric", "123.4", ""}});

  // name and description
  testServerTimingHeader("metric;desc=description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric;desc=\"description\"",
                         {{"metric", "0", "description"}});

  // name, duration, and description
  testServerTimingHeader("metric;dur=123.4;desc=description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric;desc=description;dur=123.4",
                         {{"metric", "123.4", "description"}});

  // special chars in name
  testServerTimingHeader("aB3!#$%&'*+-.^_`|~",
                         {{"aB3!#$%&'*+-.^_`|~", "0", ""}});

  // delimiter chars in quoted description
  testServerTimingHeader("metric;desc=\"descr;,=iption\";dur=123.4",
                         {{"metric", "123.4", "descr;,=iption"}});

  // spaces
  testServerTimingHeader("metric ; ", {{"metric", "0", ""}});
  testServerTimingHeader("metric , ", {{"metric", "0", ""}});
  testServerTimingHeader("metric ; dur = 123.4 ; desc = description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric ; desc = description ; dur = 123.4",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric;desc = \"description\"",
                         {{"metric", "0", "description"}});

  // tabs
  /* known failures:
  https://bugs.chromium.org/p/chromium/issues/detail?id=798446
  testServerTimingHeader("metric\t;\t", {{"metric", "0", ""}});
  testServerTimingHeader("metric\t,\t", {{"metric", "0", ""}});
  testServerTimingHeader("metric\t;\tdur\t=\t123.4\t;\tdesc\t=\tdescription",
  {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric\t;\tdesc\t=\tdescription\t;\tdur\t=\t123.4",
  {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric;desc\t=\t\"description\"", {{"metric", "0",
  "description"}});
  */

  // multiple entries
  testServerTimingHeader(
      "metric1;dur=12.3;desc=description1,metric2;dur=45.6;desc=description2,"
      "metric3;dur=78.9;desc=description3",
      {{"metric1", "12.3", "description1"},
       {"metric2", "45.6", "description2"},
       {"metric3", "78.9", "description3"}});
  testServerTimingHeader("metric1,metric2 ,metric3, metric4 , metric5",
                         {{"metric1", "0", ""},
                          {"metric2", "0", ""},
                          {"metric3", "0", ""},
                          {"metric4", "0", ""},
                          {"metric5", "0", ""}});

  // quoted-strings - happy path
  testServerTimingHeader("metric;desc=\"description\"",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric;desc=\"\t description \t\"",
                         {{"metric", "0", "\t description \t"}});
  testServerTimingHeader("metric;desc=\"descr\\\"iption\"",
                         {{"metric", "0", "descr\"iption"}});

  // quoted-strings - others
  // metric;desc=\ --> ''
  testServerTimingHeader("metric;desc=\\", {{"metric", "0", ""}});
  // metric;desc=" --> ''
  testServerTimingHeader("metric;desc=\"", {{"metric", "0", ""}});
  // metric;desc=\\ --> ''
  testServerTimingHeader("metric;desc=\\\\", {{"metric", "0", ""}});
  // metric;desc=\" --> ''
  testServerTimingHeader("metric;desc=\\\"", {{"metric", "0", ""}});
  // metric;desc="\ --> ''
  testServerTimingHeader("metric;desc=\"\\", {{"metric", "0", ""}});
  // metric;desc="" --> ''
  testServerTimingHeader("metric;desc=\"\"", {{"metric", "0", ""}});
  // metric;desc=\\\ --> ''
  testServerTimingHeader("metric;desc=\\\\\\", {{"metric", "0", ""}});
  // metric;desc=\\" --> ''
  testServerTimingHeader("metric;desc=\\\\\"", {{"metric", "0", ""}});
  // metric;desc=\"\ --> ''
  testServerTimingHeader("metric;desc=\\\"\\", {{"metric", "0", ""}});
  // metric;desc=\"" --> ''
  testServerTimingHeader("metric;desc=\\\"\"", {{"metric", "0", ""}});
  // metric;desc="\\ --> ''
  testServerTimingHeader("metric;desc=\"\\\\", {{"metric", "0", ""}});
  // metric;desc="\" --> ''
  testServerTimingHeader("metric;desc=\"\\\"", {{"metric", "0", ""}});
  // metric;desc=""\ --> ''
  testServerTimingHeader("metric;desc=\"\"\\", {{"metric", "0", ""}});
  // metric;desc=""" --> ''
  testServerTimingHeader("metric;desc=\"\"\"", {{"metric", "0", ""}});
  // metric;desc=\\\\ --> ''
  testServerTimingHeader("metric;desc=\\\\\\\\", {{"metric", "0", ""}});
  // metric;desc=\\\" --> ''
  testServerTimingHeader("metric;desc=\\\\\\\"", {{"metric", "0", ""}});
  // metric;desc=\\"\ --> ''
  testServerTimingHeader("metric;desc=\\\\\"\\", {{"metric", "0", ""}});
  // metric;desc=\\"" --> ''
  testServerTimingHeader("metric;desc=\\\\\"\"", {{"metric", "0", ""}});
  // metric;desc=\"\\ --> ''
  testServerTimingHeader("metric;desc=\\\"\\\\", {{"metric", "0", ""}});
  // metric;desc=\"\" --> ''
  testServerTimingHeader("metric;desc=\\\"\\\"", {{"metric", "0", ""}});
  // metric;desc=\""\ --> ''
  testServerTimingHeader("metric;desc=\\\"\"\\", {{"metric", "0", ""}});
  // metric;desc=\""" --> ''
  testServerTimingHeader("metric;desc=\\\"\"\"", {{"metric", "0", ""}});
  // metric;desc="\\\ --> ''
  testServerTimingHeader("metric;desc=\"\\\\\\", {{"metric", "0", ""}});
  // metric;desc="\\" --> '\'
  testServerTimingHeader("metric;desc=\"\\\\\"", {{"metric", "0", "\\"}});
  // metric;desc="\"\ --> ''
  testServerTimingHeader("metric;desc=\"\\\"\\", {{"metric", "0", ""}});
  // metric;desc="\"" --> '"'
  testServerTimingHeader("metric;desc=\"\\\"\"", {{"metric", "0", "\""}});
  // metric;desc=""\\ --> ''
  testServerTimingHeader("metric;desc=\"\"\\\\", {{"metric", "0", ""}});
  // metric;desc=""\" --> ''
  testServerTimingHeader("metric;desc=\"\"\\\"", {{"metric", "0", ""}});
  // metric;desc="""\ --> ''
  testServerTimingHeader("metric;desc=\"\"\"\\", {{"metric", "0", ""}});
  // metric;desc="""" --> ''
  testServerTimingHeader("metric;desc=\"\"\"\"", {{"metric", "0", ""}});

  // duplicate entry names
  testServerTimingHeader(
      "metric;dur=12.3;desc=description1,metric;dur=45.6;desc=description2",
      {{"metric", "12.3", "description1"}, {"metric", "45.6", "description2"}});

  // param name case sensitivity
  testServerTimingHeader("metric;DuR=123.4;DeSc=description",
                         {{"metric", "123.4", "description"}});

  // non-numeric durations
  testServerTimingHeader("metric;dur=foo", {{"metric", "0", ""}});
  testServerTimingHeader("metric;dur=\"foo\"", {{"metric", "0", ""}});

  // unrecognized param names
  testServerTimingHeader(
      "metric1;foo=bar;desc=description;foo=bar;dur=123.4;foo=bar,metric2",
      {{"metric1", "123.4", "description"}, {"metric2", "0", ""}});

  // duplicate param names
  testServerTimingHeader("metric;dur=123.4;dur=567.8",
                         {{"metric", "123.4", ""}});
  testServerTimingHeader("metric;dur=foo;dur=567.8", {{"metric", "0", ""}});
  testServerTimingHeader("metric;desc=description1;desc=description2",
                         {{"metric", "0", "description1"}});

  // incomplete params
  testServerTimingHeader("metric;dur;dur=123.4;desc=description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric;dur=;dur=123.4;desc=description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric;desc;desc=description;dur=123.4",
                         {{"metric", "123.4", ""}});
  testServerTimingHeader("metric;desc=;desc=description;dur=123.4",
                         {{"metric", "123.4", ""}});

  // extraneous characters after param value as token
  testServerTimingHeader("metric;desc=d1 d2;dur=123.4",
                         {{"metric", "123.4", "d1"}});
  testServerTimingHeader("metric1;desc=d1 d2,metric2",
                         {{"metric1", "0", "d1"}, {"metric2", "0", ""}});

  // extraneous characters after param value as quoted-string
  testServerTimingHeader("metric;desc=\"d1\" d2;dur=123.4",
                         {{"metric", "123.4", "d1"}});
  testServerTimingHeader("metric1;desc=\"d1\" d2,metric2",
                         {{"metric1", "0", "d1"}, {"metric2", "0", ""}});

  // nonsense - extraneous characters after entry name token
  testServerTimingHeader("metric==   \"\"foo;dur=123.4",
                         {{"metric", "123.4", ""}});
  testServerTimingHeader("metric1==   \"\"foo,metric2",
                         {{"metric1", "0", ""}, {"metric2", "0", ""}});

  // nonsense - extraneous characters after param name token
  testServerTimingHeader("metric;dur foo=12", {{"metric", "0", ""}});
  testServerTimingHeader("metric;foo dur=12", {{"metric", "0", ""}});

  // nonsense - return zero entries
  testServerTimingHeader(" ", {});
  testServerTimingHeader("=", {});
  testServerTimingHeader("[", {});
  testServerTimingHeader("]", {});
  testServerTimingHeader(";", {});
  testServerTimingHeader(",", {});
  testServerTimingHeader("=;", {});
  testServerTimingHeader(";=", {});
  testServerTimingHeader("=,", {});
  testServerTimingHeader(",=", {});
  testServerTimingHeader(";,", {});
  testServerTimingHeader(",;", {});
  testServerTimingHeader("=;,", {});

  // TODO(cvazac) the following tests should actually NOT pass
  // According to the definition of token/tchar
  // (https://tools.ietf.org/html/rfc7230#appendix-B),
  // HeaderFieldTokenizer.IsTokenCharacter is being too permissive for the
  // following chars (decimal):
  // 123 '{', 125 '}', and 127 (not defined)
  testServerTimingHeader("{", {{"{", "0", ""}});
  testServerTimingHeader("}", {{"}", "0", ""}});
  testServerTimingHeader("{}", {{"{}", "0", ""}});
  testServerTimingHeader("{\"foo\":\"bar\"},metric",
                         {{"{", "0", ""}, {"metric", "0", ""}});
}

TEST(HTTPParsersTest, ParseContentTypeOptionsTest) {
  struct {
    const char* value;
    ContentTypeOptionsDisposition result;
  } cases[] = {{"nosniff", kContentTypeOptionsNosniff},
               {"NOSNIFF", kContentTypeOptionsNosniff},
               {"NOsniFF", kContentTypeOptionsNosniff},
               {"nosniff, nosniff", kContentTypeOptionsNosniff},
               {"nosniff, not-nosniff", kContentTypeOptionsNosniff},
               {"nosniff, none", kContentTypeOptionsNosniff},
               {" nosniff", kContentTypeOptionsNosniff},
               {"NOSNIFF ", kContentTypeOptionsNosniff},
               {" NOsniFF ", kContentTypeOptionsNosniff},
               {" nosniff, nosniff", kContentTypeOptionsNosniff},
               {"nosniff , not-nosniff", kContentTypeOptionsNosniff},
               {" nosniff , none", kContentTypeOptionsNosniff},
               {"", kContentTypeOptionsNone},
               {",", kContentTypeOptionsNone},
               {"none", kContentTypeOptionsNone},
               {"none, nosniff", kContentTypeOptionsNone}};
  for (const auto& test : cases) {
    SCOPED_TRACE(test.value);
    EXPECT_EQ(test.result, ParseContentTypeOptionsHeader(test.value));
  }
}

// -----------------------------------------------------------------------------
// Blink's HTTP parser is reusing:
// services/network/public/cpp/content_security_policy/, which is already tested
// and fuzzed.
// What needs to be tested is the basic conversion from/to blink types.
// -----------------------------------------------------------------------------

TEST(HTTPParsersTest, ParseContentSecurityPoliciesmpty) {
  auto csp = ParseContentSecurityPolicies(
      "", network::mojom::blink::ContentSecurityPolicyType::kEnforce,
      network::mojom::blink::ContentSecurityPolicySource::kHTTP,
      KURL("http://example.com"));
  EXPECT_TRUE(csp.empty());
}

TEST(HTTPParsersTest, ParseContentSecurityPoliciesMultiple) {
  auto csp = ParseContentSecurityPolicies(
      "frame-ancestors a.com, frame-ancestors b.com",
      network::mojom::blink::ContentSecurityPolicyType::kEnforce,
      network::mojom::blink::ContentSecurityPolicySource::kHTTP,
      KURL("http://example.com"));
  ASSERT_EQ(2u, csp.size());
  EXPECT_EQ("frame-ancestors a.com", csp[0]->header->header_value);
  EXPECT_EQ("frame-ancestors b.com", csp[1]->header->header_value);
}

TEST(HTTPParsersTest, ParseContentSecurityPoliciesSingle) {
  auto csp = ParseContentSecurityPolicies(
      "frame-ancestors a.com",
      network::mojom::blink::ContentSecurityPolicyType::kEnforce,
      network::mojom::blink::ContentSecurityPolicySource::kHTTP,
      KURL("http://example.com"));
  ASSERT_EQ(1u, csp.size());

  // Header source:
  EXPECT_EQ(network::mojom::ContentSecurityPolicySource::kHTTP,
            csp[0]->header->source);

  // Header type:
  EXPECT_EQ(network::mojom::ContentSecurityPolicyType::kEnforce,
            csp[0]->header->type);

  // Header value
  EXPECT_EQ("frame-ancestors a.com", csp[0]->header->header_value);
}

TEST(HTTPParsersTest, ParseContentSecurityPoliciesMeta) {
  auto csp = ParseContentSecurityPolicies(
      "default-src a.com",
      network::mojom::blink::ContentSecurityPolicyType::kEnforce,
      network::mojom::blink::ContentSecurityPolicySource::kMeta,
      KURL("http://example.com"));
  ASSERT_EQ(1u, csp.size());

  // Header source:
  EXPECT_EQ(network::mojom::ContentSecurityPolicySource::kMeta,
            csp[0]->header->source);

  // Header type:
  EXPECT_EQ(network::mojom::ContentSecurityPolicyType::kEnforce,
            csp[0]->header->type);

  // Header value
  EXPECT_EQ("default-src a.com", csp[0]->header->header_value);
}

TEST(HTTPParsersTest, ParseContentSecurityPoliciesReportOnly) {
  auto csp = ParseContentSecurityPolicies(
      "frame-ancestors a.com",
      network::mojom::blink::ContentSecurityPolicyType::kReport,
      network::mojom::blink::ContentSecurityPolicySource::kHTTP,
      KURL("http://example.com"));
  ASSERT_EQ(1u, csp.size());

  // Header source:
  EXPECT_EQ(network::mojom::ContentSecurityPolicySource::kHTTP,
            csp[0]->header->source);

  // Header type:
  EXPECT_EQ(network::mojom::ContentSecurityPolicyType::kReport,
            csp[0]->header->type);

  // Header value
  EXPECT_EQ("frame-ancestors a.com", csp[0]->header->header_value);
}

TEST(HTTPParsersTest, ParseContentSecurityPoliciesDirectiveName) {
  auto policies = ParseContentSecurityPolicies(
      "frame-ancestors 'none', "
      "sandbox allow-script, "
      "form-action 'none', "
      "frame-src 'none', "
      "child-src 'none', "
      "script-src 'none', "
      "default-src 'none', "
      "upgrade-insecure-requests",
      network::mojom::blink::ContentSecurityPolicyType::kEnforce,
      network::mojom::blink::ContentSecurityPolicySource::kHTTP,
      KURL("http://example.com"));
  EXPECT_EQ(8u, policies.size());
  // frame-ancestors
  EXPECT_EQ(1u, policies[0]->directives.size());
  // sandbox. TODO(https://crbug.com/1041376) Implement this.
  EXPECT_EQ(0u, policies[1]->directives.size());
  // form-action.
  EXPECT_EQ(1u, policies[2]->directives.size());
  // frame-src.
  EXPECT_EQ(1u, policies[3]->directives.size());
  // child-src.
  EXPECT_EQ(1u, policies[4]->directives.size());
  // script-src.
  EXPECT_EQ(1u, policies[5]->directives.size());
  // default-src.
  EXPECT_EQ(1u, policies[6]->directives.size());
  // upgrade-insecure-policies.
  EXPECT_EQ(true, policies[7]->upgrade_insecure_requests);
}

TEST(HTTPParsersTest, ParseContentSecurityPoliciesReportTo) {
  auto policies = ParseContentSecurityPolicies(
      "report-to a b",
      network::mojom::blink::ContentSecurityPolicyType::kEnforce,
      network::mojom::blink::ContentSecurityPolicySource::kHTTP,
      KURL("http://example.com"));
  EXPECT_TRUE(policies[0]->use_reporting_api);
  // The specification https://w3c.github.io/webappsec-csp/#directive-report-to
  // only allows for one endpoints to be defined. The other ones are ignored.
  ASSERT_EQ(1u, policies[0]->report_endpoints.size());
  EXPECT_EQ("a", policies[0]->report_endpoints[0]);
}

TEST(HTTPParsersTest, ParseContentSecurityPoliciesReportUri) {
  auto policies = ParseContentSecurityPolicies(
      "report-uri ./report.py",
      network::mojom::blink::ContentSecurityPolicyType::kEnforce,
      network::mojom::blink::ContentSecurityPolicySource::kHTTP,
      KURL("http://example.com"));
  EXPECT_FALSE(policies[0]->use_reporting_api);
  ASSERT_EQ(1u, policies[0]->report_endpoints.size());
  EXPECT_EQ("http://example.com/report.py", policies[0]->report_endpoints[0]);
}

TEST(HTTPParsersTest, ParseContentSecurityPoliciesSourceBasic) {
  auto frame_ancestors = network::mojom::CSPDirectiveName::FrameAncestors;
  auto policies = ParseContentSecurityPolicies(
      "frame-ancestors 'none', "
      "frame-ancestors *, "
      "frame-ancestors 'self', "
      "frame-ancestors http://a.com:22/path, "
      "frame-ancestors a.com:*, "
      "frame-ancestors */report.py",
      network::mojom::blink::ContentSecurityPolicyType::kEnforce,
      network::mojom::blink::ContentSecurityPolicySource::kHTTP,
      KURL("http://example.com"));
  // 'none'
  {
    auto source_list = policies[0]->directives.Take(frame_ancestors);
    EXPECT_EQ(0u, source_list->sources.size());
    EXPECT_FALSE(source_list->allow_self);
    EXPECT_FALSE(source_list->allow_star);
  }

  // *
  {
    auto source_list = policies[1]->directives.Take(frame_ancestors);
    EXPECT_EQ(0u, source_list->sources.size());
    EXPECT_FALSE(source_list->allow_self);
    EXPECT_TRUE(source_list->allow_star);
  }

  // 'self'
  {
    auto source_list = policies[2]->directives.Take(frame_ancestors);
    EXPECT_EQ(0u, source_list->sources.size());
    EXPECT_TRUE(source_list->allow_self);
    EXPECT_FALSE(source_list->allow_star);
  }

  // http://a.com:22/path
  {
    auto source_list = policies[3]->directives.Take(frame_ancestors);
    EXPECT_FALSE(source_list->allow_self);
    EXPECT_FALSE(source_list->allow_star);
    EXPECT_EQ(1u, source_list->sources.size());
    auto& source = source_list->sources[0];
    EXPECT_EQ("http", source->scheme);
    EXPECT_EQ("a.com", source->host);
    EXPECT_EQ("/path", source->path);
    EXPECT_FALSE(source->is_host_wildcard);
    EXPECT_FALSE(source->is_port_wildcard);
  }

  // a.com:*
  {
    auto source_list = policies[4]->directives.Take(frame_ancestors);
    EXPECT_FALSE(source_list->allow_self);
    EXPECT_FALSE(source_list->allow_star);
    EXPECT_EQ(1u, source_list->sources.size());
    auto& source = source_list->sources[0];
    EXPECT_EQ("", source->scheme);
    EXPECT_EQ("a.com", source->host);
    EXPECT_EQ("", source->path);
    EXPECT_FALSE(source->is_host_wildcard);
    EXPECT_TRUE(source->is_port_wildcard);
  }

  // frame-ancestors */report.py
  {
    auto source_list = policies[5]->directives.Take(frame_ancestors);
    EXPECT_FALSE(source_list->allow_self);
    EXPECT_FALSE(source_list->allow_star);
    EXPECT_EQ(1u, source_list->sources.size());
    auto& source = source_list->sources[0];
    EXPECT_EQ("", source->scheme);
    EXPECT_EQ("", source->host);
    EXPECT_EQ(-1, source->port);
    EXPECT_EQ("/report.py", source->path);
    EXPECT_TRUE(source->is_host_wildcard);
    EXPECT_FALSE(source->is_port_wildcard);
  }
}

TEST(NoVarySearchPrefetchEnabledTest, ParsingNVSReturnsDefaultURLVariance) {
  const std::string_view headers =
      "HTTP/1.1 200 OK\r\n"
      "Set-Cookie: a\r\n"
      "Set-Cookie: b\r\n\r\n";
  const auto parsed_headers =
      ParseHeaders(WTF::String::FromUTF8(headers), KURL("https://a.com"));

  ASSERT_TRUE(parsed_headers);
  ASSERT_TRUE(parsed_headers->no_vary_search_with_parse_error);
  ASSERT_TRUE(
      parsed_headers->no_vary_search_with_parse_error->is_parse_error());
  EXPECT_EQ(network::mojom::NoVarySearchParseError::kOk,
            parsed_headers->no_vary_search_with_parse_error->get_parse_error());
}

struct NoVarySearchTestData {
  const char* raw_headers;
  const Vector<String> expected_no_vary_params;
  const Vector<String> expected_vary_params;
  const bool expected_vary_on_key_order;
  const bool expected_vary_by_default;
};

class NoVarySearchPrefetchEnabledTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<NoVarySearchTestData> {};

TEST_P(NoVarySearchPrefetchEnabledTest, ParsingSuccess) {
  const auto& test_data = GetParam();
  const auto parsed_headers =
      ParseHeaders(test_data.raw_headers, KURL("https://a.com"));

  ASSERT_TRUE(parsed_headers);
  ASSERT_TRUE(parsed_headers->no_vary_search_with_parse_error);
  ASSERT_TRUE(
      parsed_headers->no_vary_search_with_parse_error->is_no_vary_search());
  const auto& no_vary_search =
      parsed_headers->no_vary_search_with_parse_error->get_no_vary_search();
  ASSERT_TRUE(no_vary_search->search_variance);
  if (test_data.expected_vary_by_default) {
    EXPECT_THAT(no_vary_search->search_variance->get_no_vary_params(),
                test_data.expected_no_vary_params);
  } else {
    EXPECT_THAT(no_vary_search->search_variance->get_vary_params(),
                test_data.expected_vary_params);
  }
  EXPECT_EQ(no_vary_search->vary_on_key_order,
            test_data.expected_vary_on_key_order);
}

TEST(NoVarySearchHeaderValueParsingTest, ParsingSuccessForParseNoVarySearch) {
  const auto no_vary_search_with_parse_error =
      blink::ParseNoVarySearch(R"(params=("a"))");

  ASSERT_TRUE(no_vary_search_with_parse_error);
  ASSERT_TRUE(no_vary_search_with_parse_error->is_no_vary_search());
  ASSERT_TRUE(
      no_vary_search_with_parse_error->get_no_vary_search()->search_variance);
  EXPECT_THAT(no_vary_search_with_parse_error->get_no_vary_search()
                  ->search_variance->get_no_vary_params(),
              Vector<String>({"a"}));
  EXPECT_TRUE(
      no_vary_search_with_parse_error->get_no_vary_search()->vary_on_key_order);
}

TEST(NoVarySearchHeaderValueParsingTest, ParsingFailureForParseNoVarySearch) {
  const auto no_vary_search_with_parse_error =
      blink::ParseNoVarySearch(R"(params="a")");

  ASSERT_TRUE(no_vary_search_with_parse_error);
  EXPECT_FALSE(no_vary_search_with_parse_error->is_no_vary_search());
}

Vector<NoVarySearchTestData> GetNoVarySearchParsingSuccessTestData() {
  static Vector<NoVarySearchTestData> test_data = {
      // params set to a list of strings with one element.
      {
          "HTTP/1.1 200 OK\r\n"
          R"(No-Vary-Search: params=("a"))"
          "\r\n\r\n",             // raw_headers
          Vector<String>({"a"}),  // expected_no_vary_params
          {},                     // expected_vary_params
          true,                   // expected_vary_on_key_order
          true                    // expected_vary_by_default
      },
      // params set to true.
      {
          "HTTP/1.1 200 OK\r\n"
          "No-Vary-Search: params\r\n\r\n",  // raw_headers
          {},                                // expected_no_vary_params
          {},                                // expected_vary_params
          true,                              // expected_vary_on_key_order
          false                              // expected_vary_by_default
      },
      // Vary on one search param.
      {
          "HTTP/1.1 200 OK\r\n"
          "No-Vary-Search: params\r\n"
          R"(No-Vary-Search: except=("a"))"
          "\r\n\r\n",             // raw_headers
          {},                     // expected_no_vary_params
          Vector<String>({"a"}),  // expected_vary_params
          true,                   // expected_vary_on_key_order
          false                   // expected_vary_by_default
      },
      // Don't vary on search params order.
      {
          "HTTP/1.1 200 OK\r\n"
          "No-Vary-Search: key-order\r\n\r\n",  // raw_headers
          {},                                   // expected_no_vary_params
          {},                                   // expected_vary_params
          false,                                // expected_vary_on_key_order
          true                                  // expected_vary_by_default
      },
      // Vary on multiple search params but don't vary on search params order.
      {
          "HTTP/1.1 200 OK\r\n"
          R"(No-Vary-Search: key-order, params, except=("a" "b" "c"))"
          "\r\n\r\n",                       // raw_headers
          {},                               // expected_no_vary_params
          Vector<String>({"a", "b", "c"}),  // expected_vary_params
          false,                            // expected_vary_on_key_order
          false                             // expected_vary_by_default
      },
  };
  return test_data;
}

INSTANTIATE_TEST_SUITE_P(
    NoVarySearchPrefetchEnabledTest,
    NoVarySearchPrefetchEnabledTest,
    testing::ValuesIn(GetNoVarySearchParsingSuccessTestData()));

}  // namespace blink
