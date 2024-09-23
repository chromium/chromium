// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/data_url.h"

#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_version.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

struct ParseTestData {
  const char* url;
  bool is_valid;
  const char* mime_type;
  const char* charset;
  const std::string data;
};

}  // namespace

class DataURLTest
    : public testing::Test,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  DataURLTest() {
    using FeatureList = std::vector<base::test::FeatureRef>;
    FeatureList enabled_features;
    FeatureList disabled_features;
    const auto feature_set = [&](bool flag_on) -> FeatureList& {
      return flag_on ? enabled_features : disabled_features;
    };
    feature_set(OptimizedParsing())
        .push_back(features::kOptimizeParsingDataUrls);
    feature_set(KeepWhitespace())
        .push_back(features::kKeepWhitespaceForDataUrls);
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool OptimizedParsing() const { return std::get<0>(GetParam()); }
  bool KeepWhitespace() const { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(DataURLTest,
                         DataURLTest,
                         testing::Combine(
                             /*optimize_parsing=*/testing::Bool(),
                             /*keep_whitespace=*/testing::Bool()));

TEST_P(DataURLTest, Parse) {
  const ParseTestData tests[] = {
      {"data:", false, "", "", ""},

      {"data:,", true, "text/plain", "US-ASCII", ""},

      {"data:;base64,", true, "text/plain", "US-ASCII", ""},

      {"data:;charset=,test", false, "", "", ""},

      {"data:TeXt/HtMl,<b>x</b>", true, "text/html", "", "<b>x</b>"},

      {"data:,foo", true, "text/plain", "US-ASCII", "foo"},

      {"data:;base64,aGVsbG8gd29ybGQ=", true, "text/plain", "US-ASCII",
       "hello world"},

      // Allow invalid mediatype for backward compatibility but set mime_type to
      // "text/plain" instead of the invalid mediatype.
      {"data:foo,boo", true, "text/plain", "US-ASCII", "boo"},

      // When accepting an invalid mediatype, override charset with "US-ASCII"
      {"data:foo;charset=UTF-8,boo", true, "text/plain", "US-ASCII", "boo"},

      // Invalid mediatype. Includes a slash but the type part is not a token.
      {"data:f(oo/bar;baz=1;charset=kk,boo", true, "text/plain", "US-ASCII",
       "boo"},

      {"data:foo/bar;baz=1;charset=kk,boo", true, "foo/bar", "kk", "boo"},

      {"data:foo/bar;charset=kk;baz=1,boo", true, "foo/bar", "kk", "boo"},

      {"data:text/html,%3Chtml%3E%3Cbody%3E%3Cb%3Ehello%20world"
       "%3C%2Fb%3E%3C%2Fbody%3E%3C%2Fhtml%3E",
       true, "text/html", "", "<html><body><b>hello world</b></body></html>"},

      {"data:text/html,<html><body><b>hello world</b></body></html>", true,
       "text/html", "", "<html><body><b>hello world</b></body></html>"},

      // the comma cannot be url-escaped!
      {"data:%2Cblah", false, "", "", ""},

      // invalid base64 content
      {"data:;base64,aGVs_-_-", false, "", "", ""},

      // Spaces should NOT be removed from non-base64 encoded data URLs.
      {"data:image/fractal,a b c d e f g", true, "image/fractal", "",
       KeepWhitespace() ? "a b c d e f g" : "abcdefg"},

      // Spaces should also be removed from anything base-64 encoded
      {"data:;base64,aGVs bG8gd2  9ybGQ=", true, "text/plain", "US-ASCII",
       "hello world"},

      // Other whitespace should also be removed from anything base-64 encoded.
      {"data:;base64,aGVs bG8gd2  \n9ybGQ=", true, "text/plain", "US-ASCII",
       "hello world"},

      // In base64 encoding, escaped whitespace should be stripped.
      // (This test was taken from acid3)
      // http://b/1054495
      {"data:text/javascript;base64,%20ZD%20Qg%0D%0APS%20An%20Zm91cic%0D%0A%207"
       "%20",
       true, "text/javascript", "", "d4 = 'four';"},

      // All whitespace should be preserved on non-base64 encoded content.
      {"data:img/png,A  B  %20  %0A  C", true, "img/png", "",
       KeepWhitespace() ? "A  B     \n  C" : "AB \nC"},

      {"data:text/plain;charset=utf-8;base64,SGVsbMO2", true, "text/plain",
       "utf-8", "Hell\xC3\xB6"},

      // no mimetype
      {"data:;charset=utf-8;base64,SGVsbMO2", true, "text/plain", "utf-8",
       "Hell\xC3\xB6"},

      // Not sufficiently padded.
      {"data:;base64,aGVsbG8gd29ybGQ", true, "text/plain", "US-ASCII",
       "hello world"},

      // Not sufficiently padded with whitespace.
      {"data:;base64,aGV sbG8g d29ybGQ", true, "text/plain", "US-ASCII",
       "hello world"},

      // Not sufficiently padded with escaped whitespace.
      {"data:;base64,aGV%20sbG8g%20d29ybGQ", true, "text/plain", "US-ASCII",
       "hello world"},

      // Bad encoding (truncated).
      {"data:;base64,aGVsbG8gd29yb", false, "", "", ""},

      // BiDi control characters should be unescaped and preserved as is, and
      // should not be replaced with % versions. In the below case, \xE2\x80\x8F
      // is the RTL mark and the parsed text should preserve it as is.
      {"data:text/plain;charset=utf-8,\xE2\x80\x8Ftest", true, "text/plain",
       "utf-8", "\xE2\x80\x8Ftest"},

      // Same as above but with Arabic text after RTL mark.
      {"data:text/plain;charset=utf-8,"
       "\xE2\x80\x8F\xD8\xA7\xD8\xAE\xD8\xAA\xD8\xA8\xD8\xA7\xD8\xB1",
       true, "text/plain", "utf-8",
       "\xE2\x80\x8F\xD8\xA7\xD8\xAE\xD8\xAA\xD8\xA8\xD8\xA7\xD8\xB1"},

      // RTL mark encoded as %E2%80%8F should be unescaped too. Note that when
      // wrapped in a GURL, this URL and the next effectively become the same as
      // the previous two URLs.
      {"data:text/plain;charset=utf-8,%E2%80%8Ftest", true, "text/plain",
       "utf-8", "\xE2\x80\x8Ftest"},

      // Same as above but with Arabic text after RTL mark.
      {"data:text/plain;charset=utf-8,"
       "%E2%80%8F\xD8\xA7\xD8\xAE\xD8\xAA\xD8\xA8\xD8\xA7\xD8\xB1",
       true, "text/plain", "utf-8",
       "\xE2\x80\x8F\xD8\xA7\xD8\xAE\xD8\xAA\xD8\xA8\xD8\xA7\xD8\xB1"},

      // The 'data' of a data URI does not include any ref it has.
      {"data:text/plain,this/is/a/test/%23include/#dontinclude", true,
       "text/plain", "", "this/is/a/test/#include/"},

      // More unescaping tests and tests with nulls.
      {"data:%00text/plain%41,foo", true, "%00text/plain%41", "", "foo"},
      {"data:text/plain;charset=%00US-ASCII%41,foo", true, "text/plain",
       "%00US-ASCII%41", "foo"},
      {"data:text/plain,%00_%41", true, "text/plain", "",
       std::string("\x00_A", 3)},
      {"data:text/plain;base64,AA//", true, "text/plain", "",
       std::string("\x00\x0F\xFF", 3)},
      // "%62ase64" unescapes to base64, but should not be treated as such.
      {"data:text/plain;%62ase64,AA//", true, "text/plain", "", "AA//"},
  };

  for (const auto& test : tests) {
    SCOPED_TRACE(test.url);

    std::string mime_type;
    std::string charset;
    std::string data;
    bool ok = DataURL::Parse(GURL(test.url), &mime_type, &charset, &data);
    EXPECT_EQ(ok, test.is_valid);
    EXPECT_EQ(test.mime_type, mime_type);
    EXPECT_EQ(test.charset, charset);
    EXPECT_EQ(test.data, data);
  }
}

TEST_P(DataURLTest, BuildResponseSimple) {
  std::string mime_type;
  std::string charset;
  std::string data;
  scoped_refptr<HttpResponseHeaders> headers;

  ASSERT_EQ(OK, DataURL::BuildResponse(GURL("data:,Hello"), "GET", &mime_type,
                                       &charset, &data, &headers));

  EXPECT_EQ("text/plain", mime_type);
  EXPECT_EQ("US-ASCII", charset);
  EXPECT_EQ("Hello", data);

  ASSERT_TRUE(headers);
  const HttpVersion& version = headers->GetHttpVersion();
  EXPECT_EQ(1, version.major_value());
  EXPECT_EQ(1, version.minor_value());
  EXPECT_EQ("OK", headers->GetStatusText());
  std::string value;
  EXPECT_TRUE(headers->GetNormalizedHeader("Content-Type", &value));
  EXPECT_EQ(value, "text/plain;charset=US-ASCII");
  value.clear();
}

TEST_P(DataURLTest, BuildResponseHead) {
  for (const char* method : {"HEAD", "head", "hEaD"}) {
    SCOPED_TRACE(method);

    std::string mime_type;
    std::string charset;
    std::string data;
    scoped_refptr<HttpResponseHeaders> headers;
    ASSERT_EQ(OK,
              DataURL::BuildResponse(GURL("data:,Hello"), method, &mime_type,
                                     &charset, &data, &headers));

    EXPECT_EQ("text/plain", mime_type);
    EXPECT_EQ("US-ASCII", charset);
    EXPECT_EQ("", data);

    ASSERT_TRUE(headers);
    HttpVersion version = headers->GetHttpVersion();
    EXPECT_EQ(1, version.major_value());
    EXPECT_EQ(1, version.minor_value());
    EXPECT_EQ("OK", headers->GetStatusText());
    std::string content_type;
    EXPECT_TRUE(headers->GetNormalizedHeader("Content-Type", &content_type));
    EXPECT_EQ(content_type, "text/plain;charset=US-ASCII");
  }
}

TEST_P(DataURLTest, BuildResponseInput) {
  std::string mime_type;
  std::string charset;
  std::string data;
  scoped_refptr<HttpResponseHeaders> headers;

  ASSERT_EQ(ERR_INVALID_URL,
            DataURL::BuildResponse(GURL("bogus"), "GET", &mime_type, &charset,
                                   &data, &headers));
  EXPECT_FALSE(headers);
  EXPECT_TRUE(mime_type.empty());
  EXPECT_TRUE(charset.empty());
  EXPECT_TRUE(data.empty());
}

TEST_P(DataURLTest, BuildResponseInvalidMimeType) {
  std::string mime_type;
  std::string charset;
  std::string data;
  scoped_refptr<HttpResponseHeaders> headers;

  // MIME type contains delimiters. Must be accepted but Content-Type header
  // should be generated as if the mediatype was text/plain.
  ASSERT_EQ(OK, DataURL::BuildResponse(GURL("data:f(o/b)r,test"), "GET",
                                       &mime_type, &charset, &data, &headers));

  ASSERT_TRUE(headers);
  std::string value;
  EXPECT_TRUE(headers->GetNormalizedHeader("Content-Type", &value));
  EXPECT_EQ(value, "text/plain;charset=US-ASCII");
}

TEST_P(DataURLTest, InvalidCharset) {
  std::string mime_type;
  std::string charset;
  std::string data;
  scoped_refptr<HttpResponseHeaders> headers;

  // MIME type contains delimiters. Must be rejected.
  ASSERT_EQ(ERR_INVALID_URL, DataURL::BuildResponse(
                                 GURL("data:text/html;charset=(),test"), "GET",
                                 &mime_type, &charset, &data, &headers));
  EXPECT_FALSE(headers);
  EXPECT_TRUE(mime_type.empty());
  EXPECT_TRUE(charset.empty());
  EXPECT_TRUE(data.empty());
}

// Test a slightly larger data URL.
TEST_P(DataURLTest, Image) {
  // Use our nice little Chrome logo.
  GURL image_url(
      "data:image/png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAADVklEQVQ4jX2TfUwUB"
      "BjG3w1y+HGcd9dxhXR8T4awOccJGgOSWclHImznLkTlSw0DDQXkrmgYgbUYnlQTqQ"
      "xIEVxitD5UMCATRA1CEEg+Qjw3bWDxIauJv/5oumqs39/P827vnucRmYN0gyF01GI"
      "5MpCVdW0gO7tvNC+vqSEtbZefk5NuLv1jdJ46p/zw0HeH4+PHr3h7c1mjoV2t5rKz"
      "Mx1+fg9bAgK6zHq9cU5z+LpA3xOtx34+vTeT21onRuzssC3zxbbSwC13d/pFuC7Ck"
      "IMDxQpF7r/MWq12UctI1dWWm99ypqSYmRUBdKem8MkrO/kgaTt1O7YzlpzE5GIVd0"
      "WYUqt57yWf2McHTObYPbVD+ZwbtlLTVMZ3BW+TnLyXLaWtmEq6WJVbT3HBh3Svj2H"
      "QQcm43XwmtoYM6vVKleh0uoWvnzW3v3MpidruPTQPf0bia7sJOtBM0ufTWNvus/nk"
      "DFHF9ZS+uYVjRUasMeHUmyLYtcklTvzWGFZnNOXczThvpKIzjcahSqIzkvDLayDq6"
      "D3eOjtBbNUEIZYyqsvj4V4wY92eNJ4IoyhTbxXX1T5xsV9tm9r4TQwHLiZw/pdDZJ"
      "ea8TKmsmR/K0uLh/GwnCHghTja6lPhphezPfO5/5MrVvMzNaI3+ERHfrFzPKQukrQ"
      "GI4d/3EFD/3E2mVNYvi4at7CXWREaxZGD+3hg28zD3gVMd6q5c8GdosynKmSeRuGz"
      "pjyl1/9UDGtPR5HeaKT8Wjo17WXk579BXVUhN64ehF9fhRtq/uxxZKzNiZFGD0wRC"
      "3NFROZ5mwIPL/96K/rKMMLrIzF9uhHr+/sYH7DAbwlgC4J+R2Z7FUx1qLnV7MGF40"
      "smVSoJ/jvHRfYhQeUJd/SnYtGWhPHR0Sz+GE2F2yth0B36Vcz2KpnufBJbsysjjW4"
      "kblBUiIjiURUWqJY65zxbnTy57GQyH58zgy0QBtTQv5gH15XMdKkYu+TGaJMnlm2O"
      "34uI4b9tflqp1+QEFGzoW/ulmcofcpkZCYJhDfSpme7QcrHa+Xfji8paEQkTkSfmm"
      "oRWRNZr/F1KfVMjW+IKEnv2FwZfKdzt0BQR6lClcZR0EfEXEfv/G6W9iLiIyCoReV"
      "5EnhORIBHx+ufPj/gLB/zGI/G4Bk0AAAAASUVORK5CYII=");

  std::string mime_type;
  std::string charset;
  std::string data;
  scoped_refptr<HttpResponseHeaders> headers;

  EXPECT_EQ(OK, DataURL::BuildResponse(image_url, "GET", &mime_type, &charset,
                                       &data, &headers));

  EXPECT_EQ(911u, data.size());
  EXPECT_EQ("image/png", mime_type);
  EXPECT_TRUE(charset.empty());

  ASSERT_TRUE(headers);
  std::string value;
  EXPECT_EQ(headers->GetStatusLine(), "HTTP/1.1 200 OK");
  EXPECT_TRUE(headers->GetNormalizedHeader("Content-Type", &value));
  EXPECT_EQ(value, "image/png");
}

// Tests the application of the kRemoveWhitespaceForDataURLs command line
// switch.
TEST(DataURLRemoveWhitespaceTest, Parse) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      kRemoveWhitespaceForDataURLs);
  const ParseTestData tests[] = {
      {"data:image/fractal,a b c d e f g", true, "image/fractal", "",
       "abcdefg"},
      {"data:img/png,A  B  %20  %0A  C", true, "img/png", "", "AB \nC"},
  };

  for (const auto& test : tests) {
    SCOPED_TRACE(test.url);

    std::string mime_type;
    std::string charset;
    std::string data;
    bool ok = DataURL::Parse(GURL(test.url), &mime_type, &charset, &data);
    EXPECT_EQ(ok, test.is_valid);
    EXPECT_EQ(test.mime_type, mime_type);
    EXPECT_EQ(test.charset, charset);
    EXPECT_EQ(test.data, data);
  }
}

}  // namespace net
