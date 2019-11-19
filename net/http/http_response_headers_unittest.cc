// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_response_headers.h"

#include <stdint.h>

#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <unordered_set>

#include "base/pickle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_util.h"
#include "net/log/net_log_capture_mode.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

struct TestData {
  const char* raw_headers;
  const char* expected_headers;
  HttpVersion expected_version;
  int expected_response_code;
  const char* expected_status_text;
};

class HttpResponseHeadersTest : public testing::Test {
};

// Transform "normal"-looking headers (\n-separated) to the appropriate
// input format for ParseRawHeaders (\0-separated).
void HeadersToRaw(std::string* headers) {
  std::replace(headers->begin(), headers->end(), '\n', '\0');
  if (!headers->empty())
    *headers += '\0';
}

class HttpResponseHeadersCacheControlTest : public HttpResponseHeadersTest {
 protected:
  // Make tests less verbose.
  typedef base::TimeDelta TimeDelta;

  // Initilise the headers() value with a Cache-Control header set to
  // |cache_control|. |cache_control| is copied and so can safely be a
  // temporary.
  void InitializeHeadersWithCacheControl(const char* cache_control) {
    std::string raw_headers("HTTP/1.1 200 OK\n");
    raw_headers += "Cache-Control: ";
    raw_headers += cache_control;
    raw_headers += "\n";
    HeadersToRaw(&raw_headers);
    headers_ = new HttpResponseHeaders(raw_headers);
  }

  const scoped_refptr<HttpResponseHeaders>& headers() { return headers_; }

  // Return a pointer to a TimeDelta object. For use when the value doesn't
  // matter.
  TimeDelta* TimeDeltaPointer() { return &delta_; }

  // Get the max-age value. This should only be used in tests where a valid
  // max-age parameter is expected to be present.
  TimeDelta GetMaxAgeValue() {
    DCHECK(headers_.get()) << "Call InitializeHeadersWithCacheControl() first";
    TimeDelta max_age_value;
    EXPECT_TRUE(headers()->GetMaxAgeValue(&max_age_value));
    return max_age_value;
  }

  // Get the stale-while-revalidate value. This should only be used in tests
  // where a valid max-age parameter is expected to be present.
  TimeDelta GetStaleWhileRevalidateValue() {
    DCHECK(headers_.get()) << "Call InitializeHeadersWithCacheControl() first";
    TimeDelta stale_while_revalidate_value;
    EXPECT_TRUE(
        headers()->GetStaleWhileRevalidateValue(&stale_while_revalidate_value));
    return stale_while_revalidate_value;
  }

 private:
  scoped_refptr<HttpResponseHeaders> headers_;
  TimeDelta delta_;
};

class CommonHttpResponseHeadersTest
    : public HttpResponseHeadersTest,
      public ::testing::WithParamInterface<TestData> {
};

// Returns a simple text serialization of the given
// |HttpResponseHeaders|. This is used by tests to verify that an
// |HttpResponseHeaders| matches an expectation string.
//
//  * One line per header, written as:
//        HEADER_NAME: HEADER_VALUE\n
//  * The original case of header names is preserved.
//  * Whitespace around head names/values is stripped.
//  * Repeated headers are not aggregated.
//  * Headers are listed in their original order.
std::string ToSimpleString(const scoped_refptr<HttpResponseHeaders>& parsed) {
  std::string result = parsed->GetStatusLine() + "\n";

  size_t iter = 0;
  std::string name;
  std::string value;
  while (parsed->EnumerateHeaderLines(&iter, &name, &value)) {
    std::string new_line = name + ": " + value + "\n";

    // Verify that |name| and |value| do not contain ':' or '\n' (if they did
    // it would make this serialized format ambiguous).
    if (std::count(new_line.begin(), new_line.end(), '\n') != 1 ||
        std::count(new_line.begin(), new_line.end(), ':') != 1) {
      ADD_FAILURE() << "Unexpected characters in the header name or value: "
                    << new_line;
      return result;
    }

    result += new_line;
  }

  return result;
}

TEST_P(CommonHttpResponseHeadersTest, TestCommon) {
  const TestData test = GetParam();

  std::string raw_headers(test.raw_headers);
  HeadersToRaw(&raw_headers);
  std::string expected_headers(test.expected_headers);

  scoped_refptr<HttpResponseHeaders> parsed(
      new HttpResponseHeaders(raw_headers));
  std::string headers = ToSimpleString(parsed);

  // Transform to readable output format (so it's easier to see diffs).
  std::replace(headers.begin(), headers.end(), ' ', '_');
  std::replace(headers.begin(), headers.end(), '\n', '\\');
  std::replace(expected_headers.begin(), expected_headers.end(), ' ', '_');
  std::replace(expected_headers.begin(), expected_headers.end(), '\n', '\\');

  EXPECT_EQ(expected_headers, headers);

  EXPECT_TRUE(test.expected_version == parsed->GetHttpVersion());
  EXPECT_EQ(test.expected_response_code, parsed->response_code());
  EXPECT_EQ(test.expected_status_text, parsed->GetStatusText());
}

TestData response_headers_tests[] = {
    {// Normalize whitespace.
     "HTTP/1.1    202   Accepted  \n"
     "Content-TYPE  : text/html; charset=utf-8  \n"
     "Set-Cookie: a \n"
     "Set-Cookie:   b \n",

     "HTTP/1.1 202 Accepted\n"
     "Content-TYPE: text/html; charset=utf-8\n"
     "Set-Cookie: a\n"
     "Set-Cookie: b\n",

     HttpVersion(1, 1), 202, "Accepted"},
    {// Normalize leading whitespace.
     "HTTP/1.1    202   Accepted  \n"
     // Starts with space -- will be skipped as invalid.
     "  Content-TYPE  : text/html; charset=utf-8  \n"
     "Set-Cookie: a \n"
     "Set-Cookie:   b \n",

     "HTTP/1.1 202 Accepted\n"
     "Set-Cookie: a\n"
     "Set-Cookie: b\n",

     HttpVersion(1, 1), 202, "Accepted"},
    {// Keep whitespace within status text.
     "HTTP/1.0 404 Not   found  \n",

     "HTTP/1.0 404 Not   found\n",

     HttpVersion(1, 0), 404, "Not   found"},
    {// Normalize blank headers.
     "HTTP/1.1 200 OK\n"
     "Header1 :          \n"
     "Header2: \n"
     "Header3:\n"
     "Header4\n"
     "Header5    :\n",

     "HTTP/1.1 200 OK\n"
     "Header1: \n"
     "Header2: \n"
     "Header3: \n"
     "Header5: \n",

     HttpVersion(1, 1), 200, "OK"},
    {// Don't believe the http/0.9 version if there are headers!
     "hTtP/0.9 201\n"
     "Content-TYPE: text/html; charset=utf-8\n",

     "HTTP/1.0 201\n"
     "Content-TYPE: text/html; charset=utf-8\n",

     HttpVersion(1, 0), 201, ""},
    {// Accept the HTTP/0.9 version number if there are no headers.
     // This is how HTTP/0.9 responses get constructed from
     // HttpNetworkTransaction.
     "hTtP/0.9 200 OK\n",

     "HTTP/0.9 200 OK\n",

     HttpVersion(0, 9), 200, "OK"},
    {// Do not add missing status text.
     "HTTP/1.1 201\n"
     "Content-TYPE: text/html; charset=utf-8\n",

     "HTTP/1.1 201\n"
     "Content-TYPE: text/html; charset=utf-8\n",

     HttpVersion(1, 1), 201, ""},
    {// Normalize bad status line.
     "SCREWED_UP_STATUS_LINE\n"
     "Content-TYPE: text/html; charset=utf-8\n",

     "HTTP/1.0 200 OK\n"
     "Content-TYPE: text/html; charset=utf-8\n",

     HttpVersion(1, 0), 200, "OK"},
    {// Normalize bad status line.
     "Foo bar.",

     "HTTP/1.0 200\n",

     HttpVersion(1, 0), 200, ""},
    {// Normalize invalid status code.
     "HTTP/1.1 -1  Unknown\n",

     "HTTP/1.1 200\n",

     HttpVersion(1, 1), 200, ""},
    {// Normalize empty header.
     "",

     "HTTP/1.0 200 OK\n",

     HttpVersion(1, 0), 200, "OK"},
    {// Normalize headers that start with a colon.
     "HTTP/1.1    202   Accepted  \n"
     "foo: bar\n"
     ": a \n"
     " : b\n"
     "baz: blat \n",

     "HTTP/1.1 202 Accepted\n"
     "foo: bar\n"
     "baz: blat\n",

     HttpVersion(1, 1), 202, "Accepted"},
    {// Normalize headers that end with a colon.
     "HTTP/1.1    202   Accepted  \n"
     "foo:   \n"
     "bar:\n"
     "baz: blat \n"
     "zip:\n",

     "HTTP/1.1 202 Accepted\n"
     "foo: \n"
     "bar: \n"
     "baz: blat\n"
     "zip: \n",

     HttpVersion(1, 1), 202, "Accepted"},
    {// Normalize whitespace headers.
     "\n   \n",

     "HTTP/1.0 200 OK\n",

     HttpVersion(1, 0), 200, "OK"},
    {// Has multiple Set-Cookie headers.
     "HTTP/1.1 200 OK\n"
     "Set-Cookie: x=1\n"
     "Set-Cookie: y=2\n",

     "HTTP/1.1 200 OK\n"
     "Set-Cookie: x=1\n"
     "Set-Cookie: y=2\n",

     HttpVersion(1, 1), 200, "OK"},
    {// Has multiple cache-control headers.
     "HTTP/1.1 200 OK\n"
     "Cache-control: private\n"
     "cache-Control: no-store\n",

     "HTTP/1.1 200 OK\n"
     "Cache-control: private\n"
     "cache-Control: no-store\n",

     HttpVersion(1, 1), 200, "OK"},
};

INSTANTIATE_TEST_SUITE_P(HttpResponseHeaders,
                         CommonHttpResponseHeadersTest,
                         testing::ValuesIn(response_headers_tests));

struct PersistData {
  HttpResponseHeaders::PersistOptions options;
  const char* raw_headers;
  const char* expected_headers;
};

class PersistenceTest
    : public HttpResponseHeadersTest,
      public ::testing::WithParamInterface<PersistData> {
};

TEST_P(PersistenceTest, Persist) {
  const PersistData test = GetParam();

  std::string headers = test.raw_headers;
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed1(new HttpResponseHeaders(headers));

  base::Pickle pickle;
  parsed1->Persist(&pickle, test.options);

  base::PickleIterator iter(pickle);
  scoped_refptr<HttpResponseHeaders> parsed2(new HttpResponseHeaders(&iter));

  EXPECT_EQ(std::string(test.expected_headers), ToSimpleString(parsed2));
}

const struct PersistData persistence_tests[] = {
    {HttpResponseHeaders::PERSIST_ALL,
     "HTTP/1.1 200 OK\n"
     "Cache-control:private\n"
     "cache-Control:no-store\n",

     "HTTP/1.1 200 OK\n"
     "Cache-control: private\n"
     "cache-Control: no-store\n"},
    {HttpResponseHeaders::PERSIST_SANS_HOP_BY_HOP,
     "HTTP/1.1 200 OK\n"
     "connection: keep-alive\n"
     "server: blah\n",

     "HTTP/1.1 200 OK\n"
     "server: blah\n"},
    {HttpResponseHeaders::PERSIST_SANS_NON_CACHEABLE |
         HttpResponseHeaders::PERSIST_SANS_HOP_BY_HOP,
     "HTTP/1.1 200 OK\n"
     "fOo: 1\n"
     "Foo: 2\n"
     "Transfer-Encoding: chunked\n"
     "CoNnection: keep-alive\n"
     "cache-control: private, no-cache=\"foo\"\n",

     "HTTP/1.1 200 OK\n"
     "cache-control: private, no-cache=\"foo\"\n"},
    {HttpResponseHeaders::PERSIST_SANS_NON_CACHEABLE,
     "HTTP/1.1 200 OK\n"
     "Foo: 2\n"
     "Cache-Control: private,no-cache=\"foo, bar\"\n"
     "bar",

     "HTTP/1.1 200 OK\n"
     "Cache-Control: private,no-cache=\"foo, bar\"\n"},
    // Ignore bogus no-cache value.
    {HttpResponseHeaders::PERSIST_SANS_NON_CACHEABLE,
     "HTTP/1.1 200 OK\n"
     "Foo: 2\n"
     "Cache-Control: private,no-cache=foo\n",

     "HTTP/1.1 200 OK\n"
     "Foo: 2\n"
     "Cache-Control: private,no-cache=foo\n"},
    // Ignore bogus no-cache value.
    {HttpResponseHeaders::PERSIST_SANS_NON_CACHEABLE,
     "HTTP/1.1 200 OK\n"
     "Foo: 2\n"
     "Cache-Control: private, no-cache=\n",

     "HTTP/1.1 200 OK\n"
     "Foo: 2\n"
     "Cache-Control: private, no-cache=\n"},
    // Ignore empty no-cache value.
    {HttpResponseHeaders::PERSIST_SANS_NON_CACHEABLE,
     "HTTP/1.1 200 OK\n"
     "Foo: 2\n"
     "Cache-Control: private, no-cache=\"\"\n",

     "HTTP/1.1 200 OK\n"
     "Foo: 2\n"
     "Cache-Control: private, no-cache=\"\"\n"},
    // Ignore wrong quotes no-cache value.
    {HttpResponseHeaders::PERSIST_SANS_NON_CACHEABLE,
     "HTTP/1.1 200 OK\n"
     "Foo: 2\n"
     "Cache-Control: private, no-cache=\'foo\'\n",

     "HTTP/1.1 200 OK\n"
     "Foo: 2\n"
     "Cache-Control: private, no-cache=\'foo\'\n"},
    // Ignore unterminated quotes no-cache value.
    {HttpResponseHeaders::PERSIST_SANS_NON_CACHEABLE,
     "HTTP/1.1 200 OK\n"
     "Foo: 2\n"
     "Cache-Control: private, no-cache=\"foo\n",

     "HTTP/1.1 200 OK\n"
     "Foo: 2\n"
     "Cache-Control: private, no-cache=\"foo\n"},
    // Accept sloppy LWS.
    {HttpResponseHeaders::PERSIST_SANS_NON_CACHEABLE,
     "HTTP/1.1 200 OK\n"
     "Foo: 2\n"
     "Cache-Control: private, no-cache=\" foo\t, bar\"\n",

     "HTTP/1.1 200 OK\n"
     "Cache-Control: private, no-cache=\" foo\t, bar\"\n"},
    // Header name appears twice, separated by another header.
    {HttpResponseHeaders::PERSIST_ALL,
     "HTTP/1.1 200 OK\n"
     "Foo: 1\n"
     "Bar: 2\n"
     "Foo: 3\n",

     "HTTP/1.1 200 OK\n"
     "Foo: 1\n"
     "Bar: 2\n"
     "Foo: 3\n"},
    // Header name appears twice, separated by another header (type 2).
    {HttpResponseHeaders::PERSIST_ALL,
     "HTTP/1.1 200 OK\n"
     "Foo: 1, 3\n"
     "Bar: 2\n"
     "Foo: 4\n",

     "HTTP/1.1 200 OK\n"
     "Foo: 1, 3\n"
     "Bar: 2\n"
     "Foo: 4\n"},
    // Test filtering of cookie headers.
    {HttpResponseHeaders::PERSIST_SANS_COOKIES,
     "HTTP/1.1 200 OK\n"
     "Set-Cookie: foo=bar; httponly\n"
     "Set-Cookie: bar=foo\n"
     "Bar: 1\n"
     "Set-Cookie2: bar2=foo2\n",

     "HTTP/1.1 200 OK\n"
     "Bar: 1\n"},
    {HttpResponseHeaders::PERSIST_SANS_COOKIES,
     "HTTP/1.1 200 OK\n"
     "Set-Cookie: foo=bar\n"
     "Foo: 2\n"
     "Clear-Site-Data: { \"types\" : [ \"cookies\" ] }\n"
     "Bar: 3\n",

     "HTTP/1.1 200 OK\n"
     "Foo: 2\n"
     "Bar: 3\n"},
    // Test LWS at the end of a header.
    {HttpResponseHeaders::PERSIST_ALL,
     "HTTP/1.1 200 OK\n"
     "Content-Length: 450   \n"
     "Content-Encoding: gzip\n",

     "HTTP/1.1 200 OK\n"
     "Content-Length: 450\n"
     "Content-Encoding: gzip\n"},
    // Test LWS at the end of a header.
    {HttpResponseHeaders::PERSIST_RAW,
     "HTTP/1.1 200 OK\n"
     "Content-Length: 450   \n"
     "Content-Encoding: gzip\n",

     "HTTP/1.1 200 OK\n"
     "Content-Length: 450\n"
     "Content-Encoding: gzip\n"},
    // Test filtering of transport security state headers.
    {HttpResponseHeaders::PERSIST_SANS_SECURITY_STATE,
     "HTTP/1.1 200 OK\n"
     "Strict-Transport-Security: max-age=1576800\n"
     "Bar: 1\n",

     "HTTP/1.1 200 OK\n"
     "Bar: 1\n"},
};

INSTANTIATE_TEST_SUITE_P(HttpResponseHeaders,
                         PersistenceTest,
                         testing::ValuesIn(persistence_tests));

TEST(HttpResponseHeadersTest, EnumerateHeader_Coalesced) {
  // Ensure that commas in quoted strings are not regarded as value separators.
  // Ensure that whitespace following a value is trimmed properly.
  std::string headers =
      "HTTP/1.1 200 OK\n"
      "Cache-control:,,private , no-cache=\"set-cookie,server\",\n"
      "cache-Control: no-store\n"
      "cache-Control:\n";
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));

  size_t iter = 0;
  std::string value;
  ASSERT_TRUE(parsed->EnumerateHeader(&iter, "cache-control", &value));
  EXPECT_EQ("", value);
  ASSERT_TRUE(parsed->EnumerateHeader(&iter, "cache-control", &value));
  EXPECT_EQ("", value);
  ASSERT_TRUE(parsed->EnumerateHeader(&iter, "cache-control", &value));
  EXPECT_EQ("private", value);
  ASSERT_TRUE(parsed->EnumerateHeader(&iter, "cache-control", &value));
  EXPECT_EQ("no-cache=\"set-cookie,server\"", value);
  ASSERT_TRUE(parsed->EnumerateHeader(&iter, "cache-control", &value));
  EXPECT_EQ("", value);
  ASSERT_TRUE(parsed->EnumerateHeader(&iter, "cache-control", &value));
  EXPECT_EQ("no-store", value);
  ASSERT_TRUE(parsed->EnumerateHeader(&iter, "cache-control", &value));
  EXPECT_EQ("", value);
  EXPECT_FALSE(parsed->EnumerateHeader(&iter, "cache-control", &value));
}

TEST(HttpResponseHeadersTest, EnumerateHeader_Challenge) {
  // Even though WWW-Authenticate has commas, it should not be treated as
  // coalesced values.
  std::string headers =
      "HTTP/1.1 401 OK\n"
      "WWW-Authenticate:Digest realm=foobar, nonce=x, domain=y\n"
      "WWW-Authenticate:Basic realm=quatar\n";
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));

  size_t iter = 0;
  std::string value;
  EXPECT_TRUE(parsed->EnumerateHeader(&iter, "WWW-Authenticate", &value));
  EXPECT_EQ("Digest realm=foobar, nonce=x, domain=y", value);
  EXPECT_TRUE(parsed->EnumerateHeader(&iter, "WWW-Authenticate", &value));
  EXPECT_EQ("Basic realm=quatar", value);
  EXPECT_FALSE(parsed->EnumerateHeader(&iter, "WWW-Authenticate", &value));
}

TEST(HttpResponseHeadersTest, EnumerateHeader_DateValued) {
  // The comma in a date valued header should not be treated as a
  // field-value separator.
  std::string headers =
      "HTTP/1.1 200 OK\n"
      "Date: Tue, 07 Aug 2007 23:10:55 GMT\n"
      "Last-Modified: Wed, 01 Aug 2007 23:23:45 GMT\n";
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));

  std::string value;
  EXPECT_TRUE(parsed->EnumerateHeader(nullptr, "date", &value));
  EXPECT_EQ("Tue, 07 Aug 2007 23:10:55 GMT", value);
  EXPECT_TRUE(parsed->EnumerateHeader(nullptr, "last-modified", &value));
  EXPECT_EQ("Wed, 01 Aug 2007 23:23:45 GMT", value);
}

TEST(HttpResponseHeadersTest, DefaultDateToGMT) {
  // Verify we make the best interpretation when parsing dates that incorrectly
  // do not end in "GMT" as RFC2616 requires.
  std::string headers =
      "HTTP/1.1 200 OK\n"
      "Date: Tue, 07 Aug 2007 23:10:55\n"
      "Last-Modified: Tue, 07 Aug 2007 19:10:55 EDT\n"
      "Expires: Tue, 07 Aug 2007 23:10:55 UTC\n";
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));
  base::Time expected_value;
  ASSERT_TRUE(base::Time::FromString("Tue, 07 Aug 2007 23:10:55 GMT",
                                     &expected_value));

  base::Time value;
  // When the timezone is missing, GMT is a good guess as its what RFC2616
  // requires.
  EXPECT_TRUE(parsed->GetDateValue(&value));
  EXPECT_EQ(expected_value, value);
  // If GMT is missing but an RFC822-conforming one is present, use that.
  EXPECT_TRUE(parsed->GetLastModifiedValue(&value));
  EXPECT_EQ(expected_value, value);
  // If an unknown timezone is present, treat like a missing timezone and
  // default to GMT.  The only example of a web server not specifying "GMT"
  // used "UTC" which is equivalent to GMT.
  if (parsed->GetExpiresValue(&value))
    EXPECT_EQ(expected_value, value);
}

TEST(HttpResponseHeadersTest, GetAgeValue10) {
  std::string headers =
      "HTTP/1.1 200 OK\n"
      "Age: 10\n";
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));
  base::TimeDelta age;
  ASSERT_TRUE(parsed->GetAgeValue(&age));
  EXPECT_EQ(10, age.InSeconds());
}

TEST(HttpResponseHeadersTest, GetAgeValue0) {
  std::string headers =
      "HTTP/1.1 200 OK\n"
      "Age: 0\n";
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));
  base::TimeDelta age;
  ASSERT_TRUE(parsed->GetAgeValue(&age));
  EXPECT_EQ(0, age.InSeconds());
}

TEST(HttpResponseHeadersTest, GetAgeValueBogus) {
  std::string headers =
      "HTTP/1.1 200 OK\n"
      "Age: donkey\n";
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));
  base::TimeDelta age;
  ASSERT_FALSE(parsed->GetAgeValue(&age));
}

TEST(HttpResponseHeadersTest, GetAgeValueNegative) {
  std::string headers =
      "HTTP/1.1 200 OK\n"
      "Age: -10\n";
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));
  base::TimeDelta age;
  ASSERT_FALSE(parsed->GetAgeValue(&age));
}

TEST(HttpResponseHeadersTest, GetAgeValueLeadingPlus) {
  std::string headers =
      "HTTP/1.1 200 OK\n"
      "Age: +10\n";
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));
  base::TimeDelta age;
  ASSERT_FALSE(parsed->GetAgeValue(&age));
}

TEST(HttpResponseHeadersTest, GetAgeValueOverflow) {
  std::string headers =
      "HTTP/1.1 200 OK\n"
      "Age: 999999999999999999999999999999999999999999\n";
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));
  base::TimeDelta age;
  ASSERT_TRUE(parsed->GetAgeValue(&age));

  // Should have saturated to 2^32 - 1.
  EXPECT_EQ(static_cast<int64_t>(0xFFFFFFFFL), age.InSeconds());
}

struct ContentTypeTestData {
  const std::string raw_headers;
  const std::string mime_type;
  const bool has_mimetype;
  const std::string charset;
  const bool has_charset;
  const std::string all_content_type;
};

class ContentTypeTest
    : public HttpResponseHeadersTest,
      public ::testing::WithParamInterface<ContentTypeTestData> {
};

TEST_P(ContentTypeTest, GetMimeType) {
  const ContentTypeTestData test = GetParam();

  std::string headers(test.raw_headers);
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));

  std::string value;
  EXPECT_EQ(test.has_mimetype, parsed->GetMimeType(&value));
  EXPECT_EQ(test.mime_type, value);
  value.clear();
  EXPECT_EQ(test.has_charset, parsed->GetCharset(&value));
  EXPECT_EQ(test.charset, value);
  EXPECT_TRUE(parsed->GetNormalizedHeader("content-type", &value));
  EXPECT_EQ(test.all_content_type, value);
}

// clang-format off
const ContentTypeTestData mimetype_tests[] = {
  { "HTTP/1.1 200 OK\n"
    "Content-type: text/html\n",
    "text/html", true,
    "", false,
    "text/html" },
  // Multiple content-type headers should give us the last one.
  { "HTTP/1.1 200 OK\n"
    "Content-type: text/html\n"
    "Content-type: text/html\n",
    "text/html", true,
    "", false,
    "text/html, text/html" },
  { "HTTP/1.1 200 OK\n"
    "Content-type: text/plain\n"
    "Content-type: text/html\n"
    "Content-type: text/plain\n"
    "Content-type: text/html\n",
    "text/html", true,
    "", false,
    "text/plain, text/html, text/plain, text/html" },
  // Test charset parsing.
  { "HTTP/1.1 200 OK\n"
    "Content-type: text/html\n"
    "Content-type: text/html; charset=ISO-8859-1\n",
    "text/html", true,
    "iso-8859-1", true,
    "text/html, text/html; charset=ISO-8859-1" },
  // Test charset in double quotes.
  { "HTTP/1.1 200 OK\n"
    "Content-type: text/html\n"
    "Content-type: text/html; charset=\"ISO-8859-1\"\n",
    "text/html", true,
    "iso-8859-1", true,
    "text/html, text/html; charset=\"ISO-8859-1\"" },
  // If there are multiple matching content-type headers, we carry
  // over the charset value.
  { "HTTP/1.1 200 OK\n"
    "Content-type: text/html;charset=utf-8\n"
    "Content-type: text/html\n",
    "text/html", true,
    "utf-8", true,
    "text/html;charset=utf-8, text/html" },
  // Regression test for https://crbug.com/772350:
  // Single quotes are not delimiters but must be treated as part of charset.
  { "HTTP/1.1 200 OK\n"
    "Content-type: text/html;charset='utf-8'\n"
    "Content-type: text/html\n",
    "text/html", true,
    "'utf-8'", true,
    "text/html;charset='utf-8', text/html" },
  // First charset wins if matching content-type.
  { "HTTP/1.1 200 OK\n"
    "Content-type: text/html;charset=utf-8\n"
    "Content-type: text/html;charset=iso-8859-1\n",
    "text/html", true,
    "iso-8859-1", true,
    "text/html;charset=utf-8, text/html;charset=iso-8859-1" },
  // Charset is ignored if the content types change.
  { "HTTP/1.1 200 OK\n"
    "Content-type: text/plain;charset=utf-8\n"
    "Content-type: text/html\n",
    "text/html", true,
    "", false,
    "text/plain;charset=utf-8, text/html" },
  // Empty content-type.
  { "HTTP/1.1 200 OK\n"
    "Content-type: \n",
    "", false,
    "", false,
    "" },
  // Emtpy charset.
  { "HTTP/1.1 200 OK\n"
    "Content-type: text/html;charset=\n",
    "text/html", true,
    "", false,
    "text/html;charset=" },
  // Multiple charsets, first one wins.
  { "HTTP/1.1 200 OK\n"
    "Content-type: text/html;charset=utf-8; charset=iso-8859-1\n",
    "text/html", true,
    "utf-8", true,
    "text/html;charset=utf-8; charset=iso-8859-1" },
  // Multiple params.
  { "HTTP/1.1 200 OK\n"
    "Content-type: text/html; foo=utf-8; charset=iso-8859-1\n",
    "text/html", true,
    "iso-8859-1", true,
    "text/html; foo=utf-8; charset=iso-8859-1" },
  { "HTTP/1.1 200 OK\n"
    "Content-type: text/html ; charset=utf-8 ; bar=iso-8859-1\n",
    "text/html", true,
    "utf-8", true,
    "text/html ; charset=utf-8 ; bar=iso-8859-1" },
  // Comma embeded in quotes.
  { "HTTP/1.1 200 OK\n"
    "Content-type: text/html ; charset=\"utf-8,text/plain\" ;\n",
    "text/html", true,
    "utf-8,text/plain", true,
    "text/html ; charset=\"utf-8,text/plain\" ;" },
  // Charset with leading spaces.
  { "HTTP/1.1 200 OK\n"
    "Content-type: text/html ; charset= \"utf-8\" ;\n",
    "text/html", true,
    "utf-8", true,
    "text/html ; charset= \"utf-8\" ;" },
  // Media type comments in mime-type.
  { "HTTP/1.1 200 OK\n"
    "Content-type: text/html (html)\n",
    "text/html", true,
    "", false,
   "text/html (html)" },
  // Incomplete charset= param.
  { "HTTP/1.1 200 OK\n"
    "Content-type: text/html; char=\n",
    "text/html", true,
    "", false,
    "text/html; char=" },
  // Invalid media type: no slash.
  { "HTTP/1.1 200 OK\n"
    "Content-type: texthtml\n",
    "", false,
    "", false,
    "texthtml" },
  // Invalid media type: "*/*".
  { "HTTP/1.1 200 OK\n"
    "Content-type: */*\n",
    "", false,
    "", false,
    "*/*" },
};
// clang-format on

INSTANTIATE_TEST_SUITE_P(HttpResponseHeaders,
                         ContentTypeTest,
                         testing::ValuesIn(mimetype_tests));

struct RequiresValidationTestData {
  const char* headers;
  ValidationType validation_type;
};

class RequiresValidationTest
    : public HttpResponseHeadersTest,
      public ::testing::WithParamInterface<RequiresValidationTestData> {
};

TEST_P(RequiresValidationTest, RequiresValidation) {
  const RequiresValidationTestData test = GetParam();

  base::Time request_time, response_time, current_time;
  ASSERT_TRUE(
      base::Time::FromString("Wed, 28 Nov 2007 00:40:09 GMT", &request_time));
  ASSERT_TRUE(
      base::Time::FromString("Wed, 28 Nov 2007 00:40:12 GMT", &response_time));
  ASSERT_TRUE(
      base::Time::FromString("Wed, 28 Nov 2007 00:45:20 GMT", &current_time));

  std::string headers(test.headers);
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));

  ValidationType validation_type =
      parsed->RequiresValidation(request_time, response_time, current_time);
  EXPECT_EQ(test.validation_type, validation_type);
}

const struct RequiresValidationTestData requires_validation_tests[] = {
    // No expiry info: expires immediately.
    {"HTTP/1.1 200 OK\n"
     "\n",
     VALIDATION_SYNCHRONOUS},
    // No expiry info: expires immediately.
    {"HTTP/1.1 200 OK\n"
     "\n",
     VALIDATION_SYNCHRONOUS},
    // Valid for a little while.
    {"HTTP/1.1 200 OK\n"
     "cache-control: max-age=10000\n"
     "\n",
     VALIDATION_NONE},
    // Expires in the future.
    {"HTTP/1.1 200 OK\n"
     "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
     "expires: Wed, 28 Nov 2007 01:00:00 GMT\n"
     "\n",
     VALIDATION_NONE},
    // Already expired.
    {"HTTP/1.1 200 OK\n"
     "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
     "expires: Wed, 28 Nov 2007 00:00:00 GMT\n"
     "\n",
     VALIDATION_SYNCHRONOUS},
    // Max-age trumps expires.
    {"HTTP/1.1 200 OK\n"
     "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
     "expires: Wed, 28 Nov 2007 00:00:00 GMT\n"
     "cache-control: max-age=10000\n"
     "\n",
     VALIDATION_NONE},
    // Last-modified heuristic: modified a while ago.
    {"HTTP/1.1 200 OK\n"
     "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
     "last-modified: Wed, 27 Nov 2007 08:00:00 GMT\n"
     "\n",
     VALIDATION_NONE},
    {"HTTP/1.1 203 Non-Authoritative Information\n"
     "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
     "last-modified: Wed, 27 Nov 2007 08:00:00 GMT\n"
     "\n",
     VALIDATION_NONE},
    {"HTTP/1.1 206 Partial Content\n"
     "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
     "last-modified: Wed, 27 Nov 2007 08:00:00 GMT\n"
     "\n",
     VALIDATION_NONE},
    // Last-modified heuristic: modified recently.
    {"HTTP/1.1 200 OK\n"
     "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
     "last-modified: Wed, 28 Nov 2007 00:40:10 GMT\n"
     "\n",
     VALIDATION_SYNCHRONOUS},
    {"HTTP/1.1 203 Non-Authoritative Information\n"
     "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
     "last-modified: Wed, 28 Nov 2007 00:40:10 GMT\n"
     "\n",
     VALIDATION_SYNCHRONOUS},
    {"HTTP/1.1 206 Partial Content\n"
     "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
     "last-modified: Wed, 28 Nov 2007 00:40:10 GMT\n"
     "\n",
     VALIDATION_SYNCHRONOUS},
    // Cached permanent redirect.
    {"HTTP/1.1 301 Moved Permanently\n"
     "\n",
     VALIDATION_NONE},
    // Another cached permanent redirect.
    {"HTTP/1.1 308 Permanent Redirect\n"
     "\n",
     VALIDATION_NONE},
    // Cached redirect: not reusable even though by default it would be.
    {"HTTP/1.1 300 Multiple Choices\n"
     "Cache-Control: no-cache\n"
     "\n",
     VALIDATION_SYNCHRONOUS},
    // Cached forever by default.
    {"HTTP/1.1 410 Gone\n"
     "\n",
     VALIDATION_NONE},
    // Cached temporary redirect: not reusable.
    {"HTTP/1.1 302 Found\n"
     "\n",
     VALIDATION_SYNCHRONOUS},
    // Cached temporary redirect: reusable.
    {"HTTP/1.1 302 Found\n"
     "cache-control: max-age=10000\n"
     "\n",
     VALIDATION_NONE},
    // Cache-control: max-age=N overrides expires: date in the past.
    {"HTTP/1.1 200 OK\n"
     "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
     "expires: Wed, 28 Nov 2007 00:20:11 GMT\n"
     "cache-control: max-age=10000\n"
     "\n",
     VALIDATION_NONE},
    // Cache-control: no-store overrides expires: in the future.
    {"HTTP/1.1 200 OK\n"
     "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
     "expires: Wed, 29 Nov 2007 00:40:11 GMT\n"
     "cache-control: no-store,private,no-cache=\"foo\"\n"
     "\n",
     VALIDATION_SYNCHRONOUS},
    // Pragma: no-cache overrides last-modified heuristic.
    {"HTTP/1.1 200 OK\n"
     "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
     "last-modified: Wed, 27 Nov 2007 08:00:00 GMT\n"
     "pragma: no-cache\n"
     "\n",
     VALIDATION_SYNCHRONOUS},
    // max-age has expired, needs synchronous revalidation
    {"HTTP/1.1 200 OK\n"
     "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
     "cache-control: max-age=300\n"
     "\n",
     VALIDATION_SYNCHRONOUS},
    // max-age has expired, stale-while-revalidate has not, eligible for
    // asynchronous revalidation
    {"HTTP/1.1 200 OK\n"
     "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
     "cache-control: max-age=300, stale-while-revalidate=3600\n"
     "\n",
     VALIDATION_ASYNCHRONOUS},
    // max-age and stale-while-revalidate have expired, needs synchronous
    // revalidation
    {"HTTP/1.1 200 OK\n"
     "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
     "cache-control: max-age=300, stale-while-revalidate=5\n"
     "\n",
     VALIDATION_SYNCHRONOUS},
    // max-age is 0, stale-while-revalidate is large enough to permit
    // asynchronous revalidation
    {"HTTP/1.1 200 OK\n"
     "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
     "cache-control: max-age=0, stale-while-revalidate=360\n"
     "\n",
     VALIDATION_ASYNCHRONOUS},
    // stale-while-revalidate must not override no-cache or similar directives.
    {"HTTP/1.1 200 OK\n"
     "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
     "cache-control: no-cache, stale-while-revalidate=360\n"
     "\n",
     VALIDATION_SYNCHRONOUS},
    // max-age has not expired, so no revalidation is needed.
    {"HTTP/1.1 200 OK\n"
     "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
     "cache-control: max-age=3600, stale-while-revalidate=3600\n"
     "\n",
     VALIDATION_NONE},
    // must-revalidate overrides stale-while-revalidate, so synchronous
    // validation
    // is needed.
    {"HTTP/1.1 200 OK\n"
     "date: Wed, 28 Nov 2007 00:40:11 GMT\n"
     "cache-control: must-revalidate, max-age=300, "
     "stale-while-revalidate=3600\n"
     "\n",
     VALIDATION_SYNCHRONOUS},

    // TODO(darin): Add many many more tests here.
};

INSTANTIATE_TEST_SUITE_P(HttpResponseHeaders,
                         RequiresValidationTest,
                         testing::ValuesIn(requires_validation_tests));

struct UpdateTestData {
  const char* orig_headers;
  const char* new_headers;
  const char* expected_headers;
};

class UpdateTest
    : public HttpResponseHeadersTest,
      public ::testing::WithParamInterface<UpdateTestData> {
};

TEST_P(UpdateTest, Update) {
  const UpdateTestData test = GetParam();

  std::string orig_headers(test.orig_headers);
  HeadersToRaw(&orig_headers);
  scoped_refptr<HttpResponseHeaders> parsed(
      new HttpResponseHeaders(orig_headers));

  std::string new_headers(test.new_headers);
  HeadersToRaw(&new_headers);
  scoped_refptr<HttpResponseHeaders> new_parsed(
      new HttpResponseHeaders(new_headers));

  parsed->Update(*new_parsed.get());

  EXPECT_EQ(std::string(test.expected_headers), ToSimpleString(parsed));
}

const UpdateTestData update_tests[] = {
    {"HTTP/1.1 200 OK\n",

     "HTTP/1/1 304 Not Modified\n"
     "connection: keep-alive\n"
     "Cache-control: max-age=10000\n",

     "HTTP/1.1 200 OK\n"
     "Cache-control: max-age=10000\n"},
    {"HTTP/1.1 200 OK\n"
     "Foo: 1\n"
     "Cache-control: private\n",

     "HTTP/1/1 304 Not Modified\n"
     "connection: keep-alive\n"
     "Cache-control: max-age=10000\n",

     "HTTP/1.1 200 OK\n"
     "Cache-control: max-age=10000\n"
     "Foo: 1\n"},
    {"HTTP/1.1 200 OK\n"
     "Foo: 1\n"
     "Cache-control: private\n",

     "HTTP/1/1 304 Not Modified\n"
     "connection: keep-alive\n"
     "Cache-CONTROL: max-age=10000\n",

     "HTTP/1.1 200 OK\n"
     "Cache-CONTROL: max-age=10000\n"
     "Foo: 1\n"},
    {"HTTP/1.1 200 OK\n"
     "Content-Length: 450\n",

     "HTTP/1/1 304 Not Modified\n"
     "connection: keep-alive\n"
     "Cache-control:      max-age=10001   \n",

     "HTTP/1.1 200 OK\n"
     "Cache-control: max-age=10001\n"
     "Content-Length: 450\n"},
    {
        "HTTP/1.1 200 OK\n"
        "X-Frame-Options: DENY\n",

        "HTTP/1/1 304 Not Modified\n"
        "X-Frame-Options: ALLOW\n",

        "HTTP/1.1 200 OK\n"
        "X-Frame-Options: DENY\n",
    },
    {
        "HTTP/1.1 200 OK\n"
        "X-WebKit-CSP: default-src 'none'\n",

        "HTTP/1/1 304 Not Modified\n"
        "X-WebKit-CSP: default-src *\n",

        "HTTP/1.1 200 OK\n"
        "X-WebKit-CSP: default-src 'none'\n",
    },
    {
        "HTTP/1.1 200 OK\n"
        "X-XSS-Protection: 1\n",

        "HTTP/1/1 304 Not Modified\n"
        "X-XSS-Protection: 0\n",

        "HTTP/1.1 200 OK\n"
        "X-XSS-Protection: 1\n",
    },
    {"HTTP/1.1 200 OK\n",

     "HTTP/1/1 304 Not Modified\n"
     "X-Content-Type-Options: nosniff\n",

     "HTTP/1.1 200 OK\n"},
    {"HTTP/1.1 200 OK\n"
     "Content-Encoding: identity\n"
     "Content-Length: 100\n"
     "Content-Type: text/html\n"
     "Content-Security-Policy: default-src 'none'\n",

     "HTTP/1/1 304 Not Modified\n"
     "Content-Encoding: gzip\n"
     "Content-Length: 200\n"
     "Content-Type: text/xml\n"
     "Content-Security-Policy: default-src 'self'\n",

     "HTTP/1.1 200 OK\n"
     "Content-Security-Policy: default-src 'self'\n"
     "Content-Encoding: identity\n"
     "Content-Length: 100\n"
     "Content-Type: text/html\n"},
    {"HTTP/1.1 200 OK\n"
     "Content-Location: /example_page.html\n",

     "HTTP/1/1 304 Not Modified\n"
     "Content-Location: /not_example_page.html\n",

     "HTTP/1.1 200 OK\n"
     "Content-Location: /example_page.html\n"},
};

INSTANTIATE_TEST_SUITE_P(HttpResponseHeaders,
                         UpdateTest,
                         testing::ValuesIn(update_tests));

struct EnumerateHeaderTestData {
  const char* headers;
  const char* expected_lines;
};

class EnumerateHeaderLinesTest
    : public HttpResponseHeadersTest,
      public ::testing::WithParamInterface<EnumerateHeaderTestData> {
};

TEST_P(EnumerateHeaderLinesTest, EnumerateHeaderLines) {
  const EnumerateHeaderTestData test = GetParam();

  std::string headers(test.headers);
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));

  std::string name, value, lines;

  size_t iter = 0;
  while (parsed->EnumerateHeaderLines(&iter, &name, &value)) {
    lines.append(name);
    lines.append(": ");
    lines.append(value);
    lines.append("\n");
  }

  EXPECT_EQ(std::string(test.expected_lines), lines);
}

const EnumerateHeaderTestData enumerate_header_tests[] = {
    {"HTTP/1.1 200 OK\n",

     ""},
    {"HTTP/1.1 200 OK\n"
     "Foo: 1\n",

     "Foo: 1\n"},
    {"HTTP/1.1 200 OK\n"
     "Foo: 1\n"
     "Bar: 2\n"
     "Foo: 3\n",

     "Foo: 1\nBar: 2\nFoo: 3\n"},
    {"HTTP/1.1 200 OK\n"
     "Foo: 1, 2, 3\n",

     "Foo: 1, 2, 3\n"},
    {"HTTP/1.1 200 OK\n"
     "Foo: ,, 1,, 2, 3,, \n",

     "Foo: ,, 1,, 2, 3,,\n"},
};

INSTANTIATE_TEST_SUITE_P(HttpResponseHeaders,
                         EnumerateHeaderLinesTest,
                         testing::ValuesIn(enumerate_header_tests));

struct IsRedirectTestData {
  const char* headers;
  const char* location;
  bool is_redirect;
};

class IsRedirectTest
    : public HttpResponseHeadersTest,
      public ::testing::WithParamInterface<IsRedirectTestData> {
};

TEST_P(IsRedirectTest, IsRedirect) {
  const IsRedirectTestData test = GetParam();

  std::string headers(test.headers);
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));

  std::string location;
  EXPECT_EQ(parsed->IsRedirect(&location), test.is_redirect);
  EXPECT_EQ(location, test.location);
}

const IsRedirectTestData is_redirect_tests[] = {
  { "HTTP/1.1 200 OK\n",
    "",
    false
  },
  { "HTTP/1.1 301 Moved\n"
    "Location: http://foopy/\n",
    "http://foopy/",
    true
  },
  { "HTTP/1.1 301 Moved\n"
    "Location: \t \n",
    "",
    false
  },
  // We use the first location header as the target of the redirect.
  { "HTTP/1.1 301 Moved\n"
    "Location: http://foo/\n"
    "Location: http://bar/\n",
    "http://foo/",
    true
  },
  // We use the first _valid_ location header as the target of the redirect.
  { "HTTP/1.1 301 Moved\n"
    "Location: \n"
    "Location: http://bar/\n",
    "http://bar/",
    true
  },
  // Bug 1050541 (location header with an unescaped comma).
  { "HTTP/1.1 301 Moved\n"
    "Location: http://foo/bar,baz.html\n",
    "http://foo/bar,baz.html",
    true
  },
  // Bug 1224617 (location header with non-ASCII bytes).
  { "HTTP/1.1 301 Moved\n"
    "Location: http://foo/bar?key=\xE4\xF6\xFC\n",
    "http://foo/bar?key=%E4%F6%FC",
    true
  },
  // Shift_JIS, Big5, and GBK contain multibyte characters with the trailing
  // byte falling in the ASCII range.
  { "HTTP/1.1 301 Moved\n"
    "Location: http://foo/bar?key=\x81\x5E\xD8\xBF\n",
    "http://foo/bar?key=%81^%D8%BF",
    true
  },
  { "HTTP/1.1 301 Moved\n"
    "Location: http://foo/bar?key=\x82\x40\xBD\xC4\n",
    "http://foo/bar?key=%82@%BD%C4",
    true
  },
  { "HTTP/1.1 301 Moved\n"
    "Location: http://foo/bar?key=\x83\x5C\x82\x5D\xCB\xD7\n",
    "http://foo/bar?key=%83\\%82]%CB%D7",
    true
  },
};

INSTANTIATE_TEST_SUITE_P(HttpResponseHeaders,
                         IsRedirectTest,
                         testing::ValuesIn(is_redirect_tests));

struct ContentLengthTestData {
  const char* headers;
  int64_t expected_len;
};

class GetContentLengthTest
    : public HttpResponseHeadersTest,
      public ::testing::WithParamInterface<ContentLengthTestData> {
};

TEST_P(GetContentLengthTest, GetContentLength) {
  const ContentLengthTestData test = GetParam();

  std::string headers(test.headers);
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));

  EXPECT_EQ(test.expected_len, parsed->GetContentLength());
}

const ContentLengthTestData content_length_tests[] = {
    {"HTTP/1.1 200 OK\n", -1},
    {"HTTP/1.1 200 OK\n"
     "Content-Length: 10\n",
     10},
    {"HTTP/1.1 200 OK\n"
     "Content-Length: \n",
     -1},
    {"HTTP/1.1 200 OK\n"
     "Content-Length: abc\n",
     -1},
    {"HTTP/1.1 200 OK\n"
     "Content-Length: -10\n",
     -1},
    {"HTTP/1.1 200 OK\n"
     "Content-Length:  +10\n",
     -1},
    {"HTTP/1.1 200 OK\n"
     "Content-Length: 23xb5\n",
     -1},
    {"HTTP/1.1 200 OK\n"
     "Content-Length: 0xA\n",
     -1},
    {"HTTP/1.1 200 OK\n"
     "Content-Length: 010\n",
     10},
    // Content-Length too big, will overflow an int64_t.
    {"HTTP/1.1 200 OK\n"
     "Content-Length: 40000000000000000000\n",
     -1},
    {"HTTP/1.1 200 OK\n"
     "Content-Length:       10\n",
     10},
    {"HTTP/1.1 200 OK\n"
     "Content-Length: 10  \n",
     10},
    {"HTTP/1.1 200 OK\n"
     "Content-Length: \t10\n",
     10},
    {"HTTP/1.1 200 OK\n"
     "Content-Length: \v10\n",
     -1},
    {"HTTP/1.1 200 OK\n"
     "Content-Length: \f10\n",
     -1},
    {"HTTP/1.1 200 OK\n"
     "cOnTeNt-LENgth: 33\n",
     33},
    {"HTTP/1.1 200 OK\n"
     "Content-Length: 34\r\n",
     -1},
};

INSTANTIATE_TEST_SUITE_P(HttpResponseHeaders,
                         GetContentLengthTest,
                         testing::ValuesIn(content_length_tests));

struct ContentRangeTestData {
  const char* headers;
  bool expected_return_value;
  int64_t expected_first_byte_position;
  int64_t expected_last_byte_position;
  int64_t expected_instance_size;
};

class ContentRangeTest
    : public HttpResponseHeadersTest,
      public ::testing::WithParamInterface<ContentRangeTestData> {
};

TEST_P(ContentRangeTest, GetContentRangeFor206) {
  const ContentRangeTestData test = GetParam();

  std::string headers(test.headers);
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));

  int64_t first_byte_position;
  int64_t last_byte_position;
  int64_t instance_size;
  bool return_value = parsed->GetContentRangeFor206(
      &first_byte_position, &last_byte_position, &instance_size);
  EXPECT_EQ(test.expected_return_value, return_value);
  EXPECT_EQ(test.expected_first_byte_position, first_byte_position);
  EXPECT_EQ(test.expected_last_byte_position, last_byte_position);
  EXPECT_EQ(test.expected_instance_size, instance_size);
}

const ContentRangeTestData content_range_tests[] = {
    {"HTTP/1.1 206 Partial Content", false, -1, -1, -1},
    {"HTTP/1.1 206 Partial Content\n"
     "Content-Range:",
     false, -1, -1, -1},
    {"HTTP/1.1 206 Partial Content\n"
     "Content-Range: bytes 0-50/51",
     true, 0, 50, 51},
    {"HTTP/1.1 206 Partial Content\n"
     "Content-Range: bytes 50-0/51",
     false, -1, -1, -1},
    {"HTTP/1.1 416 Requested range not satisfiable\n"
     "Content-Range: bytes */*",
     false, -1, -1, -1},
    {"HTTP/1.1 206 Partial Content\n"
     "Content-Range: bytes 0-50/*",
     false, -1, -1, -1},
};

INSTANTIATE_TEST_SUITE_P(HttpResponseHeaders,
                         ContentRangeTest,
                         testing::ValuesIn(content_range_tests));

struct KeepAliveTestData {
  const char* headers;
  bool expected_keep_alive;
};

// Enable GTest to print KeepAliveTestData in an intelligible way if the test
// fails.
void PrintTo(const KeepAliveTestData& keep_alive_test_data,
             std::ostream* os) {
  *os << "{\"" << keep_alive_test_data.headers << "\", " << std::boolalpha
      << keep_alive_test_data.expected_keep_alive << "}";
}

class IsKeepAliveTest
    : public HttpResponseHeadersTest,
      public ::testing::WithParamInterface<KeepAliveTestData> {
};

TEST_P(IsKeepAliveTest, IsKeepAlive) {
  const KeepAliveTestData test = GetParam();

  std::string headers(test.headers);
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));

  EXPECT_EQ(test.expected_keep_alive, parsed->IsKeepAlive());
}

const KeepAliveTestData keepalive_tests[] = {
  // The status line fabricated by HttpNetworkTransaction for a 0.9 response.
  // Treated as 0.9.
  { "HTTP/0.9 200 OK",
    false
  },
  // This could come from a broken server.  Treated as 1.0 because it has a
  // header.
  { "HTTP/0.9 200 OK\n"
    "connection: keep-alive\n",
    true
  },
  { "HTTP/1.1 200 OK\n",
    true
  },
  { "HTTP/1.0 200 OK\n",
    false
  },
  { "HTTP/1.0 200 OK\n"
    "connection: close\n",
    false
  },
  { "HTTP/1.0 200 OK\n"
    "connection: keep-alive\n",
    true
  },
  { "HTTP/1.0 200 OK\n"
    "connection: kEeP-AliVe\n",
    true
  },
  { "HTTP/1.0 200 OK\n"
    "connection: keep-aliveX\n",
    false
  },
  { "HTTP/1.1 200 OK\n"
    "connection: close\n",
    false
  },
  { "HTTP/1.1 200 OK\n"
    "connection: keep-alive\n",
    true
  },
  { "HTTP/1.0 200 OK\n"
    "proxy-connection: close\n",
    false
  },
  { "HTTP/1.0 200 OK\n"
    "proxy-connection: keep-alive\n",
    true
  },
  { "HTTP/1.1 200 OK\n"
    "proxy-connection: close\n",
    false
  },
  { "HTTP/1.1 200 OK\n"
    "proxy-connection: keep-alive\n",
    true
  },
  { "HTTP/1.1 200 OK\n"
    "Connection: Upgrade, close\n",
    false
  },
  { "HTTP/1.1 200 OK\n"
    "Connection: Upgrade, keep-alive\n",
    true
  },
  { "HTTP/1.1 200 OK\n"
    "Connection: Upgrade\n"
    "Connection: close\n",
    false
  },
  { "HTTP/1.1 200 OK\n"
    "Connection: Upgrade\n"
    "Connection: keep-alive\n",
    true
  },
  { "HTTP/1.1 200 OK\n"
    "Connection: close, Upgrade\n",
    false
  },
  { "HTTP/1.1 200 OK\n"
    "Connection: keep-alive, Upgrade\n",
    true
  },
  { "HTTP/1.1 200 OK\n"
    "Connection: Upgrade\n"
    "Proxy-Connection: close\n",
    false
  },
  { "HTTP/1.1 200 OK\n"
    "Connection: Upgrade\n"
    "Proxy-Connection: keep-alive\n",
    true
  },
  // In situations where the response headers conflict with themselves, use the
  // first one for backwards-compatibility.
  { "HTTP/1.1 200 OK\n"
    "Connection: close\n"
    "Connection: keep-alive\n",
    false
  },
  { "HTTP/1.1 200 OK\n"
    "Connection: keep-alive\n"
    "Connection: close\n",
    true
  },
  { "HTTP/1.0 200 OK\n"
    "Connection: close\n"
    "Connection: keep-alive\n",
    false
  },
  { "HTTP/1.0 200 OK\n"
    "Connection: keep-alive\n"
    "Connection: close\n",
    true
  },
  // Ignore the Proxy-Connection header if at all possible.
  { "HTTP/1.0 200 OK\n"
    "Proxy-Connection: keep-alive\n"
    "Connection: close\n",
    false
  },
  { "HTTP/1.1 200 OK\n"
    "Proxy-Connection: close\n"
    "Connection: keep-alive\n",
    true
  },
  // Older versions of Chrome would have ignored Proxy-Connection in this case,
  // but it doesn't seem safe.
  { "HTTP/1.1 200 OK\n"
    "Proxy-Connection: close\n"
    "Connection: Transfer-Encoding\n",
    false
  },
};

INSTANTIATE_TEST_SUITE_P(HttpResponseHeaders,
                         IsKeepAliveTest,
                         testing::ValuesIn(keepalive_tests));

struct HasStrongValidatorsTestData {
  const char* headers;
  bool expected_result;
};

class HasStrongValidatorsTest
    : public HttpResponseHeadersTest,
      public ::testing::WithParamInterface<HasStrongValidatorsTestData> {
};

TEST_P(HasStrongValidatorsTest, HasStrongValidators) {
  const HasStrongValidatorsTestData test = GetParam();

  std::string headers(test.headers);
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));

  EXPECT_EQ(test.expected_result, parsed->HasStrongValidators());
}

const HasStrongValidatorsTestData strong_validators_tests[] = {
  { "HTTP/0.9 200 OK",
    false
  },
  { "HTTP/1.0 200 OK\n"
    "Date: Wed, 28 Nov 2007 01:40:10 GMT\n"
    "Last-Modified: Wed, 28 Nov 2007 00:40:10 GMT\n"
    "ETag: \"foo\"\n",
    false
  },
  { "HTTP/1.1 200 OK\n"
    "Date: Wed, 28 Nov 2007 01:40:10 GMT\n"
    "Last-Modified: Wed, 28 Nov 2007 00:40:10 GMT\n"
    "ETag: \"foo\"\n",
    true
  },
  { "HTTP/1.1 200 OK\n"
    "Date: Wed, 28 Nov 2007 00:41:10 GMT\n"
    "Last-Modified: Wed, 28 Nov 2007 00:40:10 GMT\n",
    true
  },
  { "HTTP/1.1 200 OK\n"
    "Date: Wed, 28 Nov 2007 00:41:09 GMT\n"
    "Last-Modified: Wed, 28 Nov 2007 00:40:10 GMT\n",
    false
  },
  { "HTTP/1.1 200 OK\n"
    "ETag: \"foo\"\n",
    true
  },
  // This is not really a weak etag:
  { "HTTP/1.1 200 OK\n"
    "etag: \"w/foo\"\n",
    true
  },
  // This is a weak etag:
  { "HTTP/1.1 200 OK\n"
    "etag: w/\"foo\"\n",
    false
  },
  { "HTTP/1.1 200 OK\n"
    "etag:    W  /   \"foo\"\n",
    false
  }
};

INSTANTIATE_TEST_SUITE_P(HttpResponseHeaders,
                         HasStrongValidatorsTest,
                         testing::ValuesIn(strong_validators_tests));

TEST(HttpResponseHeadersTest, HasValidatorsNone) {
  std::string headers("HTTP/1.1 200 OK");
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));
  EXPECT_FALSE(parsed->HasValidators());
}

TEST(HttpResponseHeadersTest, HasValidatorsEtag) {
  std::string headers(
      "HTTP/1.1 200 OK\n"
      "etag: \"anything\"");
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));
  EXPECT_TRUE(parsed->HasValidators());
}

TEST(HttpResponseHeadersTest, HasValidatorsLastModified) {
  std::string headers(
      "HTTP/1.1 200 OK\n"
      "Last-Modified: Wed, 28 Nov 2007 00:40:10 GMT");
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));
  EXPECT_TRUE(parsed->HasValidators());
}

TEST(HttpResponseHeadersTest, HasValidatorsWeakEtag) {
  std::string headers(
      "HTTP/1.1 200 OK\n"
      "etag: W/\"anything\"");
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));
  EXPECT_TRUE(parsed->HasValidators());
}

TEST(HttpResponseHeadersTest, GetNormalizedHeaderWithEmptyValues) {
  std::string headers(
      "HTTP/1.1 200 OK\n"
      "a:\n"
      "b: \n"
      "c:*\n"
      "d: *\n"
      "e:    \n"
      "a: \n"
      "b:*\n"
      "c:\n"
      "d:*\n"
      "a:\n");
  HeadersToRaw(&headers);
  auto parsed = base::MakeRefCounted<HttpResponseHeaders>(headers);
  std::string value;

  EXPECT_TRUE(parsed->GetNormalizedHeader("a", &value));
  EXPECT_EQ(value, ", , ");
  EXPECT_TRUE(parsed->GetNormalizedHeader("b", &value));
  EXPECT_EQ(value, ", *");
  EXPECT_TRUE(parsed->GetNormalizedHeader("c", &value));
  EXPECT_EQ(value, "*, ");
  EXPECT_TRUE(parsed->GetNormalizedHeader("d", &value));
  EXPECT_EQ(value, "*, *");
  EXPECT_TRUE(parsed->GetNormalizedHeader("e", &value));
  EXPECT_EQ(value, "");
  EXPECT_FALSE(parsed->GetNormalizedHeader("f", &value));
}

TEST(HttpResponseHeadersTest, GetNormalizedHeaderWithCommas) {
  std::string headers(
      "HTTP/1.1 200 OK\n"
      "a: foo, bar\n"
      "b: , foo, bar,\n"
      "c: ,,,\n"
      "d:  ,  ,  ,  \n"
      "e:\t,\t,\t,\t\n"
      "a: ,");
  HeadersToRaw(&headers);
  auto parsed = base::MakeRefCounted<HttpResponseHeaders>(headers);
  std::string value;

  // TODO(mmenke): "Normalized" headers probably should preserve the
  // leading/trailing whitespace from the original headers.
  ASSERT_TRUE(parsed->GetNormalizedHeader("a", &value));
  EXPECT_EQ("foo, bar, ,", value);
  ASSERT_TRUE(parsed->GetNormalizedHeader("b", &value));
  EXPECT_EQ(", foo, bar,", value);
  ASSERT_TRUE(parsed->GetNormalizedHeader("c", &value));
  EXPECT_EQ(",,,", value);
  ASSERT_TRUE(parsed->GetNormalizedHeader("d", &value));
  EXPECT_EQ(",  ,  ,", value);
  ASSERT_TRUE(parsed->GetNormalizedHeader("e", &value));
  EXPECT_EQ(",\t,\t,", value);
  EXPECT_FALSE(parsed->GetNormalizedHeader("f", &value));
}

struct AddHeaderTestData {
  const char* orig_headers;
  const char* new_header;
  const char* expected_headers;
};

class AddHeaderTest
    : public HttpResponseHeadersTest,
      public ::testing::WithParamInterface<AddHeaderTestData> {
};

TEST_P(AddHeaderTest, AddHeader) {
  const AddHeaderTestData test = GetParam();

  std::string orig_headers(test.orig_headers);
  HeadersToRaw(&orig_headers);
  scoped_refptr<HttpResponseHeaders> parsed(
      new HttpResponseHeaders(orig_headers));

  std::string new_header(test.new_header);
  parsed->AddHeader(new_header);

  EXPECT_EQ(std::string(test.expected_headers), ToSimpleString(parsed));
}

const AddHeaderTestData add_header_tests[] = {
  { "HTTP/1.1 200 OK\n"
    "connection: keep-alive\n"
    "Cache-control: max-age=10000\n",

    "Content-Length: 450",

    "HTTP/1.1 200 OK\n"
    "connection: keep-alive\n"
    "Cache-control: max-age=10000\n"
    "Content-Length: 450\n"
  },
  { "HTTP/1.1 200 OK\n"
    "connection: keep-alive\n"
    "Cache-control: max-age=10000    \n",

    "Content-Length: 450  ",

    "HTTP/1.1 200 OK\n"
    "connection: keep-alive\n"
    "Cache-control: max-age=10000\n"
    "Content-Length: 450\n"
  },
};

INSTANTIATE_TEST_SUITE_P(HttpResponseHeaders,
                         AddHeaderTest,
                         testing::ValuesIn(add_header_tests));

struct RemoveHeaderTestData {
  const char* orig_headers;
  const char* to_remove;
  const char* expected_headers;
};

class RemoveHeaderTest
    : public HttpResponseHeadersTest,
      public ::testing::WithParamInterface<RemoveHeaderTestData> {
};

TEST_P(RemoveHeaderTest, RemoveHeader) {
  const RemoveHeaderTestData test = GetParam();

  std::string orig_headers(test.orig_headers);
  HeadersToRaw(&orig_headers);
  scoped_refptr<HttpResponseHeaders> parsed(
      new HttpResponseHeaders(orig_headers));

  std::string name(test.to_remove);
  parsed->RemoveHeader(name);

  EXPECT_EQ(std::string(test.expected_headers), ToSimpleString(parsed));
}

const RemoveHeaderTestData remove_header_tests[] = {
  { "HTTP/1.1 200 OK\n"
    "connection: keep-alive\n"
    "Cache-control: max-age=10000\n"
    "Content-Length: 450\n",

    "Content-Length",

    "HTTP/1.1 200 OK\n"
    "connection: keep-alive\n"
    "Cache-control: max-age=10000\n"
  },
  { "HTTP/1.1 200 OK\n"
    "connection: keep-alive  \n"
    "Content-Length  : 450  \n"
    "Cache-control: max-age=10000\n",

    "Content-Length",

    "HTTP/1.1 200 OK\n"
    "connection: keep-alive\n"
    "Cache-control: max-age=10000\n"
  },
};

INSTANTIATE_TEST_SUITE_P(HttpResponseHeaders,
                         RemoveHeaderTest,
                         testing::ValuesIn(remove_header_tests));

struct RemoveHeadersTestData {
  const char* orig_headers;
  const char* to_remove[2];
  const char* expected_headers;
};

class RemoveHeadersTest
    : public HttpResponseHeadersTest,
      public ::testing::WithParamInterface<RemoveHeadersTestData> {};

TEST_P(RemoveHeadersTest, RemoveHeaders) {
  const RemoveHeadersTestData test = GetParam();

  std::string orig_headers(test.orig_headers);
  HeadersToRaw(&orig_headers);
  scoped_refptr<HttpResponseHeaders> parsed(
      new HttpResponseHeaders(orig_headers));

  std::unordered_set<std::string> to_remove;
  for (auto* header : test.to_remove) {
    if (header)
      to_remove.insert(header);
  }
  parsed->RemoveHeaders(to_remove);

  EXPECT_EQ(std::string(test.expected_headers), ToSimpleString(parsed));
}

const RemoveHeadersTestData remove_headers_tests[] = {
    {"HTTP/1.1 200 OK\n"
     "connection: keep-alive\n"
     "Cache-control: max-age=10000\n"
     "Content-Length: 450\n",

     {"Content-Length", "CACHE-control"},

     "HTTP/1.1 200 OK\n"
     "connection: keep-alive\n"},

    {"HTTP/1.1 200 OK\n"
     "connection: keep-alive\n"
     "Content-Length: 450\n",

     {"foo", "bar"},

     "HTTP/1.1 200 OK\n"
     "connection: keep-alive\n"
     "Content-Length: 450\n"},

    {"HTTP/1.1 404 Kinda not OK\n"
     "connection: keep-alive  \n",

     {},

     "HTTP/1.1 404 Kinda not OK\n"
     "connection: keep-alive\n"},
};

INSTANTIATE_TEST_SUITE_P(HttpResponseHeaders,
                         RemoveHeadersTest,
                         testing::ValuesIn(remove_headers_tests));

struct RemoveIndividualHeaderTestData {
  const char* orig_headers;
  const char* to_remove_name;
  const char* to_remove_value;
  const char* expected_headers;
};

class RemoveIndividualHeaderTest
    : public HttpResponseHeadersTest,
      public ::testing::WithParamInterface<RemoveIndividualHeaderTestData> {
};

TEST_P(RemoveIndividualHeaderTest, RemoveIndividualHeader) {
  const RemoveIndividualHeaderTestData test = GetParam();

  std::string orig_headers(test.orig_headers);
  HeadersToRaw(&orig_headers);
  scoped_refptr<HttpResponseHeaders> parsed(
      new HttpResponseHeaders(orig_headers));

  std::string name(test.to_remove_name);
  std::string value(test.to_remove_value);
  parsed->RemoveHeaderLine(name, value);

  EXPECT_EQ(std::string(test.expected_headers), ToSimpleString(parsed));
}

const RemoveIndividualHeaderTestData remove_individual_header_tests[] = {
  { "HTTP/1.1 200 OK\n"
    "connection: keep-alive\n"
    "Cache-control: max-age=10000\n"
    "Content-Length: 450\n",

    "Content-Length",

    "450",

    "HTTP/1.1 200 OK\n"
    "connection: keep-alive\n"
    "Cache-control: max-age=10000\n"
  },
  { "HTTP/1.1 200 OK\n"
    "connection: keep-alive  \n"
    "Content-Length  : 450  \n"
    "Cache-control: max-age=10000\n",

    "Content-Length",

    "450",

    "HTTP/1.1 200 OK\n"
    "connection: keep-alive\n"
    "Cache-control: max-age=10000\n"
  },
  { "HTTP/1.1 200 OK\n"
    "connection: keep-alive  \n"
    "Content-Length: 450\n"
    "Cache-control: max-age=10000\n",

    "Content-Length",  // Matching name.

    "999",  // Mismatching value.

    "HTTP/1.1 200 OK\n"
    "connection: keep-alive\n"
    "Content-Length: 450\n"
    "Cache-control: max-age=10000\n"
  },
  { "HTTP/1.1 200 OK\n"
    "connection: keep-alive  \n"
    "Foo: bar, baz\n"
    "Foo: bar\n"
    "Cache-control: max-age=10000\n",

    "Foo",

    "bar, baz",  // Space in value.

    "HTTP/1.1 200 OK\n"
    "connection: keep-alive\n"
    "Foo: bar\n"
    "Cache-control: max-age=10000\n"
  },
  { "HTTP/1.1 200 OK\n"
    "connection: keep-alive  \n"
    "Foo: bar, baz\n"
    "Cache-control: max-age=10000\n",

    "Foo",

    "baz",  // Only partial match -> ignored.

    "HTTP/1.1 200 OK\n"
    "connection: keep-alive\n"
    "Foo: bar, baz\n"
    "Cache-control: max-age=10000\n"
  },
};

INSTANTIATE_TEST_SUITE_P(HttpResponseHeaders,
                         RemoveIndividualHeaderTest,
                         testing::ValuesIn(remove_individual_header_tests));

struct ReplaceStatusTestData {
  const char* orig_headers;
  const char* new_status;
  const char* expected_headers;
};

class ReplaceStatusTest
    : public HttpResponseHeadersTest,
      public ::testing::WithParamInterface<ReplaceStatusTestData> {
};

TEST_P(ReplaceStatusTest, ReplaceStatus) {
  const ReplaceStatusTestData test = GetParam();

  std::string orig_headers(test.orig_headers);
  HeadersToRaw(&orig_headers);
  scoped_refptr<HttpResponseHeaders> parsed(
      new HttpResponseHeaders(orig_headers));

  std::string name(test.new_status);
  parsed->ReplaceStatusLine(name);

  EXPECT_EQ(std::string(test.expected_headers), ToSimpleString(parsed));
}

const ReplaceStatusTestData replace_status_tests[] = {
  { "HTTP/1.1 206 Partial Content\n"
    "connection: keep-alive\n"
    "Cache-control: max-age=10000\n"
    "Content-Length: 450\n",

    "HTTP/1.1 200 OK",

    "HTTP/1.1 200 OK\n"
    "connection: keep-alive\n"
    "Cache-control: max-age=10000\n"
    "Content-Length: 450\n"
  },
  { "HTTP/1.1 200 OK\n"
    "connection: keep-alive\n",

    "HTTP/1.1 304 Not Modified",

    "HTTP/1.1 304 Not Modified\n"
    "connection: keep-alive\n"
  },
  { "HTTP/1.1 200 OK\n"
    "connection: keep-alive  \n"
    "Content-Length  : 450   \n"
    "Cache-control: max-age=10000\n",

    "HTTP/1//1 304 Not Modified",

    "HTTP/1.0 304 Not Modified\n"
    "connection: keep-alive\n"
    "Content-Length: 450\n"
    "Cache-control: max-age=10000\n"
  },
};

INSTANTIATE_TEST_SUITE_P(HttpResponseHeaders,
                         ReplaceStatusTest,
                         testing::ValuesIn(replace_status_tests));

struct UpdateWithNewRangeTestData {
  const char* orig_headers;
  const char* expected_headers;
  const char* expected_headers_with_replaced_status;
};

class UpdateWithNewRangeTest
    : public HttpResponseHeadersTest,
      public ::testing::WithParamInterface<UpdateWithNewRangeTestData> {
};

TEST_P(UpdateWithNewRangeTest, UpdateWithNewRange) {
  const UpdateWithNewRangeTestData test = GetParam();

  const HttpByteRange range = HttpByteRange::Bounded(3, 5);

  std::string orig_headers(test.orig_headers);
  std::replace(orig_headers.begin(), orig_headers.end(), '\n', '\0');
  scoped_refptr<HttpResponseHeaders> parsed(
      new HttpResponseHeaders(orig_headers + '\0'));
  int64_t content_size = parsed->GetContentLength();

  // Update headers without replacing status line.
  parsed->UpdateWithNewRange(range, content_size, false);
  EXPECT_EQ(std::string(test.expected_headers), ToSimpleString(parsed));

  // Replace status line too.
  parsed->UpdateWithNewRange(range, content_size, true);
  EXPECT_EQ(std::string(test.expected_headers_with_replaced_status),
            ToSimpleString(parsed));
}

const UpdateWithNewRangeTestData update_range_tests[] = {
  { "HTTP/1.1 200 OK\n"
    "Content-Length: 450\n",

    "HTTP/1.1 200 OK\n"
    "Content-Range: bytes 3-5/450\n"
    "Content-Length: 3\n",

    "HTTP/1.1 206 Partial Content\n"
    "Content-Range: bytes 3-5/450\n"
    "Content-Length: 3\n",
  },
  { "HTTP/1.1 200 OK\n"
    "Content-Length: 5\n",

    "HTTP/1.1 200 OK\n"
    "Content-Range: bytes 3-5/5\n"
    "Content-Length: 3\n",

    "HTTP/1.1 206 Partial Content\n"
    "Content-Range: bytes 3-5/5\n"
    "Content-Length: 3\n",
  },
};

INSTANTIATE_TEST_SUITE_P(HttpResponseHeaders,
                         UpdateWithNewRangeTest,
                         testing::ValuesIn(update_range_tests));

TEST_F(HttpResponseHeadersCacheControlTest, AbsentMaxAgeReturnsFalse) {
  InitializeHeadersWithCacheControl("nocache");
  EXPECT_FALSE(headers()->GetMaxAgeValue(TimeDeltaPointer()));
}

TEST_F(HttpResponseHeadersCacheControlTest, MaxAgeWithNoParameterRejected) {
  InitializeHeadersWithCacheControl("max-age=,private");
  EXPECT_FALSE(headers()->GetMaxAgeValue(TimeDeltaPointer()));
}

TEST_F(HttpResponseHeadersCacheControlTest, MaxAgeWithSpaceParameterRejected) {
  InitializeHeadersWithCacheControl("max-age= ,private");
  EXPECT_FALSE(headers()->GetMaxAgeValue(TimeDeltaPointer()));
}

TEST_F(HttpResponseHeadersCacheControlTest,
       MaxAgeWithSpaceBeforeEqualsIsRejected) {
  InitializeHeadersWithCacheControl("max-age = 7");
  EXPECT_FALSE(headers()->GetMaxAgeValue(TimeDeltaPointer()));
}

TEST_F(HttpResponseHeadersCacheControlTest, MaxAgeFirstMatchUsed) {
  InitializeHeadersWithCacheControl("max-age=10, max-age=20");
  EXPECT_EQ(TimeDelta::FromSeconds(10), GetMaxAgeValue());
}

TEST_F(HttpResponseHeadersCacheControlTest, MaxAgeBogusFirstMatchUsed) {
  // "max-age10" isn't parsed as "max-age"; "max-age=now" is parsed as
  // "max-age=0" and so "max-age=20" is not used.
  InitializeHeadersWithCacheControl("max-age10, max-age=now, max-age=20");
  EXPECT_EQ(TimeDelta::FromSeconds(0), GetMaxAgeValue());
}

TEST_F(HttpResponseHeadersCacheControlTest, MaxAgeCaseInsensitive) {
  InitializeHeadersWithCacheControl("Max-aGe=15");
  EXPECT_EQ(TimeDelta::FromSeconds(15), GetMaxAgeValue());
}

struct MaxAgeTestData {
  const char* max_age_string;
  const int64_t expected_seconds;
};

class MaxAgeEdgeCasesTest
    : public HttpResponseHeadersCacheControlTest,
      public ::testing::WithParamInterface<MaxAgeTestData> {
};

TEST_P(MaxAgeEdgeCasesTest, MaxAgeEdgeCases) {
  const MaxAgeTestData test = GetParam();

  std::string max_age = "max-age=";
  InitializeHeadersWithCacheControl(
      (max_age + test.max_age_string).c_str());
  EXPECT_EQ(test.expected_seconds, GetMaxAgeValue().InSeconds())
      << " for max-age=" << test.max_age_string;
}

const MaxAgeTestData max_age_tests[] = {
    {" 1 ", 1},  // Spaces are ignored.
    {"-1", -1},  // Negative numbers are passed through.
    {"--1", 0},  // Leading junk gives 0.
    {"2s", 2},   // Trailing junk is ignored.
    {"3 days", 3},
    {"'4'", 0},    // Single quotes don't work.
    {"\"5\"", 0},  // Double quotes don't work.
    {"0x6", 0},    // Hex not parsed as hex.
    {"7F", 7},     // Hex without 0x still not parsed as hex.
    {"010", 10},   // Octal not parsed as octal.
    {"9223372036854", 9223372036854},
    //  {"9223372036855", -9223372036854},  // Undefined behaviour.
    //  {"9223372036854775806", -2},        // Undefined behaviour.
    {"9223372036854775807", 9223372036854775807},
    {"20000000000000000000",
     std::numeric_limits<int64_t>::max()},  // Overflow int64_t.
};

INSTANTIATE_TEST_SUITE_P(HttpResponseHeadersCacheControl,
                         MaxAgeEdgeCasesTest,
                         testing::ValuesIn(max_age_tests));

TEST_F(HttpResponseHeadersCacheControlTest,
       AbsentStaleWhileRevalidateReturnsFalse) {
  InitializeHeadersWithCacheControl("max-age=3600");
  EXPECT_FALSE(headers()->GetStaleWhileRevalidateValue(TimeDeltaPointer()));
}

TEST_F(HttpResponseHeadersCacheControlTest,
       StaleWhileRevalidateWithoutValueRejected) {
  InitializeHeadersWithCacheControl("max-age=3600,stale-while-revalidate=");
  EXPECT_FALSE(headers()->GetStaleWhileRevalidateValue(TimeDeltaPointer()));
}

TEST_F(HttpResponseHeadersCacheControlTest,
       StaleWhileRevalidateWithInvalidValueTreatedAsZero) {
  InitializeHeadersWithCacheControl("max-age=3600,stale-while-revalidate=true");
  EXPECT_EQ(TimeDelta(), GetStaleWhileRevalidateValue());
}

TEST_F(HttpResponseHeadersCacheControlTest, StaleWhileRevalidateValueReturned) {
  InitializeHeadersWithCacheControl("max-age=3600,stale-while-revalidate=7200");
  EXPECT_EQ(TimeDelta::FromSeconds(7200), GetStaleWhileRevalidateValue());
}

TEST_F(HttpResponseHeadersCacheControlTest,
       FirstStaleWhileRevalidateValueUsed) {
  InitializeHeadersWithCacheControl(
      "stale-while-revalidate=1,stale-while-revalidate=7200");
  EXPECT_EQ(TimeDelta::FromSeconds(1), GetStaleWhileRevalidateValue());
}

struct GetCurrentAgeTestData {
  const char* headers;
  const char* request_time;
  const char* response_time;
  const char* current_time;
  const int expected_age;
};

class GetCurrentAgeTest
    : public HttpResponseHeadersTest,
      public ::testing::WithParamInterface<GetCurrentAgeTestData> {
};

TEST_P(GetCurrentAgeTest, GetCurrentAge) {
  const GetCurrentAgeTestData test = GetParam();

  base::Time request_time, response_time, current_time;
  ASSERT_TRUE(base::Time::FromString(test.request_time, &request_time));
  ASSERT_TRUE(base::Time::FromString(test.response_time, &response_time));
  ASSERT_TRUE(base::Time::FromString(test.current_time, &current_time));

  std::string headers(test.headers);
  HeadersToRaw(&headers);
  scoped_refptr<HttpResponseHeaders> parsed(new HttpResponseHeaders(headers));

  base::TimeDelta age =
      parsed->GetCurrentAge(request_time, response_time, current_time);
  EXPECT_EQ(test.expected_age, age.InSeconds());
}

const struct GetCurrentAgeTestData get_current_age_tests[] = {
    // Without Date header.
    {"HTTP/1.1 200 OK\n"
     "Age: 2",
     "Fri, 20 Jan 2011 10:40:08 GMT", "Fri, 20 Jan 2011 10:40:12 GMT",
     "Fri, 20 Jan 2011 10:40:14 GMT", 8},
    // Without Age header.
    {"HTTP/1.1 200 OK\n"
     "Date: Fri, 20 Jan 2011 10:40:10 GMT\n",
     "Fri, 20 Jan 2011 10:40:08 GMT", "Fri, 20 Jan 2011 10:40:12 GMT",
     "Fri, 20 Jan 2011 10:40:14 GMT", 6},
    // date_value > response_time with Age header.
    {"HTTP/1.1 200 OK\n"
     "Date: Fri, 20 Jan 2011 10:40:14 GMT\n"
     "Age: 2\n",
     "Fri, 20 Jan 2011 10:40:08 GMT", "Fri, 20 Jan 2011 10:40:12 GMT",
     "Fri, 20 Jan 2011 10:40:14 GMT", 8},
     // date_value > response_time without Age header.
     {"HTTP/1.1 200 OK\n"
     "Date: Fri, 20 Jan 2011 10:40:14 GMT\n",
     "Fri, 20 Jan 2011 10:40:08 GMT", "Fri, 20 Jan 2011 10:40:12 GMT",
     "Fri, 20 Jan 2011 10:40:14 GMT", 6},
    // apparent_age > corrected_age_value
    {"HTTP/1.1 200 OK\n"
     "Date: Fri, 20 Jan 2011 10:40:07 GMT\n"
     "Age: 0\n",
     "Fri, 20 Jan 2011 10:40:08 GMT", "Fri, 20 Jan 2011 10:40:12 GMT",
     "Fri, 20 Jan 2011 10:40:14 GMT", 7}};

INSTANTIATE_TEST_SUITE_P(HttpResponseHeaders,
                         GetCurrentAgeTest,
                         testing::ValuesIn(get_current_age_tests));

}  // namespace

}  // namespace net
