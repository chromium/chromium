// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/network/http_parsers.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/wtf/dtoa/utils.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

TEST(HTTPParsersTest, ParseCacheControl) {
  CacheControlHeader header;

  header = ParseCacheControlDirectives("no-cache", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_TRUE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_TRUE(std::isnan(header.max_age));
  EXPECT_TRUE(std::isnan(header.stale_while_revalidate));

  header = ParseCacheControlDirectives("no-cache no-store", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_TRUE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_TRUE(std::isnan(header.max_age));
  EXPECT_TRUE(std::isnan(header.stale_while_revalidate));

  header =
      ParseCacheControlDirectives("no-store must-revalidate", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_FALSE(header.contains_no_cache);
  EXPECT_TRUE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_TRUE(std::isnan(header.max_age));
  EXPECT_TRUE(std::isnan(header.stale_while_revalidate));

  header = ParseCacheControlDirectives("max-age=0", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_FALSE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_EQ(0.0, header.max_age);
  EXPECT_TRUE(std::isnan(header.stale_while_revalidate));

  header = ParseCacheControlDirectives("max-age", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_FALSE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_TRUE(std::isnan(header.max_age));
  EXPECT_TRUE(std::isnan(header.stale_while_revalidate));

  header = ParseCacheControlDirectives("max-age=0 no-cache", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_FALSE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_EQ(0.0, header.max_age);
  EXPECT_TRUE(std::isnan(header.stale_while_revalidate));

  header = ParseCacheControlDirectives("no-cache=foo", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_FALSE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_TRUE(std::isnan(header.max_age));
  EXPECT_TRUE(std::isnan(header.stale_while_revalidate));

  header = ParseCacheControlDirectives("nonsense", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_FALSE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_TRUE(std::isnan(header.max_age));
  EXPECT_TRUE(std::isnan(header.stale_while_revalidate));

  header = ParseCacheControlDirectives("\rno-cache\n\t\v\0\b", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_TRUE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_TRUE(std::isnan(header.max_age));
  EXPECT_TRUE(std::isnan(header.stale_while_revalidate));

  header = ParseCacheControlDirectives("      no-cache       ", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_TRUE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_TRUE(std::isnan(header.max_age));
  EXPECT_TRUE(std::isnan(header.stale_while_revalidate));

  header = ParseCacheControlDirectives(AtomicString(), "no-cache");
  EXPECT_TRUE(header.parsed);
  EXPECT_TRUE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_TRUE(std::isnan(header.max_age));
  EXPECT_TRUE(std::isnan(header.stale_while_revalidate));

  header = ParseCacheControlDirectives(
      "stale-while-revalidate=2,stale-while-revalidate=3", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_FALSE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_TRUE(std::isnan(header.max_age));
  EXPECT_EQ(2.0, header.stale_while_revalidate);
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
  double delay;
  String url;
  EXPECT_FALSE(ParseHTTPRefresh("", nullptr, delay, url));
  EXPECT_FALSE(ParseHTTPRefresh(" ", nullptr, delay, url));
  EXPECT_FALSE(ParseHTTPRefresh("1.3xyz url=foo", nullptr, delay, url));
  EXPECT_FALSE(ParseHTTPRefresh("1.3.4xyz url=foo", nullptr, delay, url));
  EXPECT_FALSE(ParseHTTPRefresh("1e1 url=foo", nullptr, delay, url));

  EXPECT_TRUE(ParseHTTPRefresh("123 ", nullptr, delay, url));
  EXPECT_EQ(123.0, delay);
  EXPECT_TRUE(url.IsEmpty());

  EXPECT_TRUE(ParseHTTPRefresh("1 ; url=dest", nullptr, delay, url));
  EXPECT_EQ(1.0, delay);
  EXPECT_EQ("dest", url);
  EXPECT_TRUE(
      ParseHTTPRefresh("1 ;\nurl=dest", IsASCIISpace<UChar>, delay, url));
  EXPECT_EQ(1.0, delay);
  EXPECT_EQ("dest", url);
  EXPECT_TRUE(ParseHTTPRefresh("1 ;\nurl=dest", nullptr, delay, url));
  EXPECT_EQ(1.0, delay);
  EXPECT_EQ("url=dest", url);

  EXPECT_TRUE(ParseHTTPRefresh("1 url=dest", nullptr, delay, url));
  EXPECT_EQ(1.0, delay);
  EXPECT_EQ("dest", url);

  EXPECT_TRUE(
      ParseHTTPRefresh("10\nurl=dest", IsASCIISpace<UChar>, delay, url));
  EXPECT_EQ(10, delay);
  EXPECT_EQ("dest", url);

  EXPECT_TRUE(
      ParseHTTPRefresh("1.5; url=dest", IsASCIISpace<UChar>, delay, url));
  EXPECT_EQ(1.5, delay);
  EXPECT_EQ("dest", url);
  EXPECT_TRUE(
      ParseHTTPRefresh("1.5.9; url=dest", IsASCIISpace<UChar>, delay, url));
  EXPECT_EQ(1.5, delay);
  EXPECT_EQ("dest", url);
  EXPECT_TRUE(
      ParseHTTPRefresh("7..; url=dest", IsASCIISpace<UChar>, delay, url));
  EXPECT_EQ(7, delay);
  EXPECT_EQ("dest", url);
}

TEST(HTTPParsersTest, ParseMultipartHeadersResult) {
  struct {
    const char* data;
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
  for (size_t i = 0; i < arraysize(tests); ++i) {
    ResourceResponse response;
    wtf_size_t end = 0;
    bool result = ParseMultipartHeadersFromBody(
        tests[i].data, static_cast<wtf_size_t>(strlen(tests[i].data)),
        &response, &end);
    EXPECT_EQ(tests[i].result, result);
    EXPECT_EQ(tests[i].end, end);
  }
}

TEST(HTTPParsersTest, ParseMultipartHeaders) {
  ResourceResponse response;
  response.AddHTTPHeaderField("foo", "bar");
  response.AddHTTPHeaderField("range", "piyo");
  response.AddHTTPHeaderField("content-length", "999");

  const char kData[] = "content-type: image/png\ncontent-length: 10\n\n";
  wtf_size_t end = 0;
  bool result =
      ParseMultipartHeadersFromBody(kData, strlen(kData), &response, &end);

  EXPECT_TRUE(result);
  EXPECT_EQ(strlen(kData), end);
  EXPECT_EQ("image/png", response.HttpHeaderField("content-type"));
  EXPECT_EQ("10", response.HttpHeaderField("content-length"));
  EXPECT_EQ("bar", response.HttpHeaderField("foo"));
  EXPECT_EQ(AtomicString(), response.HttpHeaderField("range"));
}

TEST(HTTPParsersTest, ParseMultipartHeadersContentCharset) {
  ResourceResponse response;
  const char kData[] = "content-type: text/html; charset=utf-8\n\n";
  wtf_size_t end = 0;
  bool result =
      ParseMultipartHeadersFromBody(kData, strlen(kData), &response, &end);

  EXPECT_TRUE(result);
  EXPECT_EQ(strlen(kData), end);
  EXPECT_EQ("text/html; charset=utf-8",
            response.HttpHeaderField("content-type"));
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
  testServerTimingHeader("metric==   \"\"foo;dur=123.4", {{"metric", "0", ""}});
  testServerTimingHeader("metric1==   \"\"foo,metric2", {{"metric1", "0", ""}});

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
  testServerTimingHeader("{\"foo\":\"bar\"},metric", {{"{", "0", ""}});
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
               {"none", kContentTypeOptionsNone},
               {"none, nosniff", kContentTypeOptionsNone}};
  for (const auto& test : cases) {
    SCOPED_TRACE(test.value);
    EXPECT_EQ(test.result, ParseContentTypeOptionsHeader(test.value));
  }
}

TEST(HTTPParsersTest, ParseXSSProtectionDirectives) {
  // clang-format off
  struct {
    const char* input;
    ReflectedXSSDisposition expected_result;
    const char* expected_failure_reason;
    unsigned expected_failure_position;
    const char* expected_report_url;
    unsigned line;
  } tests[] = {

#undef SUCC__
#undef FAIL__
#define SUCC__(a, b, d, e) { a, b, nullptr, d, e, __LINE__ }
#define FAIL__(a, c, d) {a, kReflectedXSSInvalid, c, d, nullptr, __LINE__}

      // Empty header.
      SUCC__("", kReflectedXSSUnset, 0, nullptr),
      SUCC__("     ", kReflectedXSSUnset, 0, nullptr),

      // First positional parameter tests.
      SUCC__("0", kAllowReflectedXSS, 0, nullptr),
      SUCC__("  0  ", kAllowReflectedXSS, 0, nullptr),
      SUCC__("  0 ;", kAllowReflectedXSS, 0, nullptr),
      SUCC__("1", kFilterReflectedXSS, 0, nullptr),
      SUCC__("  1  ", kFilterReflectedXSS, 0, nullptr),
      SUCC__("  1 ;", kFilterReflectedXSS, 0, nullptr),
      FAIL__(";", "expected token to be 0 or 1", 0),
      FAIL__(";;", "expected token to be 0 or 1", 0),
      FAIL__(";;;", "expected token to be 0 or 1", 0),
      FAIL__("  ;  ", "expected token to be 0 or 1", 0),
      FAIL__(" ; ; ", "expected token to be 0 or 1", 0),
      FAIL__("; ; ;", "expected token to be 0 or 1", 0),
      FAIL__("2", "expected token to be 0 or 1", 0),
      FAIL__("  2  ", "expected token to be 0 or 1", 0),
      FAIL__("-1", "expected token to be 0 or 1", 0),
      FAIL__(" -1  ", "expected token to be 0 or 1", 0),
      FAIL__("red", "expected token to be 0 or 1", 0),
      FAIL__("12345678901234567", "expected token to be 0 or 1", 0),
      FAIL__("1:", "expected semicolon", 1),
      FAIL__("1 2", "expected semicolon", 2),
      FAIL__("1; red", "unrecognized directive", 3),

      // Don't allow quoted strings here.
      FAIL__("\"", "expected token to be 0 or 1", 0),
      FAIL__("\"0", "expected token to be 0 or 1", 0),
      FAIL__("\"0\"", "expected token to be 0 or 1", 0),
      FAIL__("\" 0 \"", "expected token to be 0 or 1", 0),
      FAIL__("\" 0\";", "expected token to be 0 or 1", 0),
      FAIL__("\" 0;\"", "expected token to be 0 or 1", 0),
      FAIL__("\"1", "expected token to be 0 or 1", 0),
      FAIL__("\"1\"", "expected token to be 0 or 1", 0),
      FAIL__("\" 1 \"", "expected token to be 0 or 1", 0),
      FAIL__("\" 1\";", "expected token to be 0 or 1", 0),
      FAIL__("\" 1;\"", "expected token to be 0 or 1", 0),

      // No other parameters unless enabled.
      SUCC__("0; mode=block; report=http://u:p@x.com:n/f.x?q=3&v=%26#tag",
             kAllowReflectedXSS, 0, nullptr),
      SUCC__("0; all sorts of nonsense; \"here\"",
             kAllowReflectedXSS, 0, nullptr),

      // Mode parameter tests.
      SUCC__("1;mode=block", kBlockReflectedXSS, 0, nullptr),
      SUCC__("1; mode=block;", kBlockReflectedXSS, 0, nullptr),
      SUCC__("1; mode=block; ", kBlockReflectedXSS, 0, nullptr),
      SUCC__("1; mode=block ;", kBlockReflectedXSS, 0, nullptr),
      SUCC__("1; mode= block;", kBlockReflectedXSS, 0, nullptr),
      SUCC__("1; mode =block;", kBlockReflectedXSS, 0, nullptr),
      FAIL__("1; mode", "expected equals sign", 7),
      FAIL__("1; mode=", "invalid mode directive", 8),
      FAIL__("1; mode =", "invalid mode directive", 9),
      FAIL__("1; mode=purple", "invalid mode directive", 8),
      FAIL__("1; mode=block-a-block-block", "invalid mode directive", 8),
      FAIL__("1; mode=block=a-block-block", "expected semicolon", 13),
      FAIL__("1; mode=block; mode=block", "duplicate mode directive", 15),
      FAIL__("1; mode=block; report=foo; mode=block;",
             "duplicate mode directive", 27),

      // Quoted strings allowed here.
      SUCC__("1; mode=\"block\"", kBlockReflectedXSS, 0, nullptr),
      SUCC__("1; mode=\"block\";", kBlockReflectedXSS, 0, nullptr),
      FAIL__("1; mode=\"block;\"", "invalid mode directive", 8),
      FAIL__("1; mode=\"red\"", "invalid mode directive", 8),

      // Report url parameter tests.
      SUCC__("1;report=http://u:p@x.com:n/f.x?q=3&v=%26#tag",
             kFilterReflectedXSS, 9, "http://u:p@x.com:n/f.x?q=3&v=%26#tag"),
      SUCC__("1; report=http://u:p@x.com:n/f.x?q=3&v=%26#tag",
             kFilterReflectedXSS, 10, "http://u:p@x.com:n/f.x?q=3&v=%26#tag"),
      SUCC__("1; report=http://u:p@x.com:n/f.x?q=3&v=%26#tag ",
             kFilterReflectedXSS, 10, "http://u:p@x.com:n/f.x?q=3&v=%26#tag"),
      SUCC__("1; report= http://u:p@x.com:n/f.x?q=3&v=%26#tag",
             kFilterReflectedXSS, 11, "http://u:p@x.com:n/f.x?q=3&v=%26#tag"),
      SUCC__("1; report =http://u:p@x.com:n/f.x?q=3&v=%26#tag",
             kFilterReflectedXSS, 11, "http://u:p@x.com:n/f.x?q=3&v=%26#tag"),
      SUCC__("1; report=http://u:p@x.com:n/f.x?q=3&v=%26#tag;",
             kFilterReflectedXSS, 10, "http://u:p@x.com:n/f.x?q=3&v=%26#tag"),
      SUCC__("1; mode=block; report=http://u:p@x.com:n/f.x?q=3&v=%26#tag",
             kBlockReflectedXSS, 22, "http://u:p@x.com:n/f.x?q=3&v=%26#tag"),
      SUCC__("1; report=http://u:p@x.com:n/f.x?q=3&v=%26#tag; mode=block",
             kBlockReflectedXSS, 10, "http://u:p@x.com:n/f.x?q=3&v=%26#tag"),
      FAIL__("1; report", "expected equals sign", 9),
      FAIL__("1; report=", "invalid report directive", 10),
      FAIL__("1; report= ;", "invalid report directive", 11),
      FAIL__("1; report=http://foo.com; mode=block; report=http://foo.com;",
             "duplicate report directive", 38),
      FAIL__("1; report=http://foo.com;SEQUID=7", "unrecognized directive", 25),
      FAIL__("1; mode=block; report=http://foo.com;SEQUID=7",
             "unrecognized directive", 37),

      // Quoted strings allowed per https://crbug.com/825557
      SUCC__("1; report=\"http://foo.com\"", kFilterReflectedXSS, 10,
             "http://foo.com"),
      SUCC__("1; report=\"http://foo.com;SEQUID=7\"", kFilterReflectedXSS, 10,
             "http://foo.com;SEQUID=7"),
      FAIL__("1; report=\"", "invalid report directive", 10),
      FAIL__("1; report=\"http://foo.com", "invalid report directive", 10),
      FAIL__("1; report=\"http://foo.com\";SEQUID=7",
             "unrecognized directive", 27),

#undef SUCC__
#undef FAIL__
  };
  // clang-format on

  for (const auto& test : tests) {
    String report_url;
    String failure_reason;
    unsigned failure_position = 0;
    ReflectedXSSDisposition result = ParseXSSProtectionHeader(
        test.input, failure_reason, failure_position, report_url);
    EXPECT_EQ(test.expected_result, result)
        << "for testcase at line " << test.line;
    if (result == kReflectedXSSInvalid) {
      EXPECT_EQ(test.expected_failure_reason, failure_reason)
          << "for testcase at line " << test.line;
    } else {
      EXPECT_EQ(test.expected_report_url, report_url)
          << "for testcase at line " << test.line;
    }
    EXPECT_EQ(test.expected_failure_position, failure_position)
        << "for testcase at line " << test.line;
  }
}

}  // namespace blink
