// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/text_encoding_detector.h"

#include <string>
#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "url/url_util.h"

namespace blink {

namespace {

// BOM-less UTF-8 Korean text ("한글 테스트입니다."), as found in a plain text
// file created by a typical Android text editor.
constexpr char kUtf8KoreanText[] =
    "\xED\x95\x9C\xEA\xB8\x80 \xED\x85\x8C\xEC\x8A\xA4\xED\x8A\xB8"
    "\xEC\x9E\x85\xEB\x8B\x88\xEB\x8B\xA4.";

}  // namespace

TEST(TextEncodingDetectorTest, RespectIso2022Jp) {
  // ISO-2022-JP is the only 7-bit encoding defined in WHATWG standard.
  std::string iso2022jp =
      " \x1B"
      "$BKL3$F;F|K\\%O%`%U%!%$%?!<%:$,%=%U%H%P%s%/$H$N%W%l!<%*%U$r@)$7!\"";
  TextEncoding encoding;
  bool result = DetectTextEncoding(base::as_byte_span(iso2022jp), nullptr,
                                   NullUrl(), nullptr, &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(TextEncoding("ISO-2022-JP"), encoding);
}

TEST(TextEncodingDetectorTest, Ignore7BitEncoding) {
  // 7-bit encodings except ISO-2022-JP are not supported by WHATWG.
  // They should be detected as plain text (US-ASCII).
  std::string hz_gb2312 =
      " ~{\x54\x42\x31\x7D\x37\x22\x55\x39\x35\x3D\x3D\x71~} abc";
  TextEncoding encoding;
  bool result = DetectTextEncoding(base::as_byte_span(hz_gb2312), nullptr,
                                   NullUrl(), nullptr, &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(TextEncoding("US-ASCII"), encoding);
}

TEST(TextEncodingDetectorTest, NonWhatwgEncodingBecomesAscii) {
  std::string pseudo_jpg =
      "\xff\xd8\xff\xe0\x00\x10JFIF foo bar baz\xff\xe1\x00\xa5"
      "\x01\xd7\xff\x01\x57\x33\x44\x55\x66\x77\xed\xcb\xa9\x87"
      "\xff\xd7\xff\xe0\x00\x10JFIF foo bar baz\xff\xe1\x00\xa5"
      "\x87\x01\xd7\xff\x01\x57\x33\x44\x55\x66\x77\xed\xcb\xa9";
  TextEncoding encoding;
  bool result = DetectTextEncoding(base::as_byte_span(pseudo_jpg), nullptr,
                                   NullUrl(), nullptr, &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(TextEncoding("US-ASCII"), encoding);
}

TEST(TextEncodingDetectorTest, UrlHintHelpsEucJp) {
  std::string eucjp_bytes =
      "<TITLE>"
      "\xA5\xD1\xA5\xEF\xA1\xBC\xA5\xC1\xA5\xE3\xA1\xBC\xA5\xC8\xA1\xC3\xC5\xEA"
      "\xBB\xF1\xBE\xF0\xCA\xF3\xA4\xCE\xA5\xD5\xA5\xA3\xA5\xB9\xA5\xB3</"
      "TITLE>";
  TextEncoding encoding;
  bool result = DetectTextEncoding(base::as_byte_span(eucjp_bytes), nullptr,
                                   NullUrl(), nullptr, &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(TextEncoding("GBK"), encoding)
      << "Without language hint, it's detected as GBK";

  KURL url_jp_domain("http://example.co.jp/");
  result = DetectTextEncoding(base::as_byte_span(eucjp_bytes), nullptr,
                              url_jp_domain, nullptr, &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(TextEncoding("EUC-JP"), encoding)
      << "With URL hint including '.jp', it's detected as EUC-JP";
}

TEST(TextEncodingDetectorTest, LanguageHintHelpsEucJp) {
  std::string eucjp_bytes =
      "<TITLE>"
      "\xA5\xD1\xA5\xEF\xA1\xBC\xA5\xC1\xA5\xE3\xA1\xBC\xA5\xC8\xA1\xC3\xC5\xEA"
      "\xBB\xF1\xBE\xF0\xCA\xF3\xA4\xCE\xA5\xD5\xA5\xA3\xA5\xB9\xA5\xB3</"
      "TITLE>";
  TextEncoding encoding;
  bool result = DetectTextEncoding(base::as_byte_span(eucjp_bytes), nullptr,
                                   NullUrl(), nullptr, &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(TextEncoding("GBK"), encoding)
      << "Without language hint, it's detected as GBK";

  KURL url("http://example.com/");
  result = DetectTextEncoding(base::as_byte_span(eucjp_bytes), nullptr, url,
                              "ja", &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(TextEncoding("GBK"), encoding)
      << "Language hint doesn't help for normal URL. Should be detected as GBK";

  KURL file_url("file:///text.txt");
  result = DetectTextEncoding(base::as_byte_span(eucjp_bytes), nullptr,
                              file_url, "ja", &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(TextEncoding("EUC-JP"), encoding)
      << "Language hint works for file resource. Should be detected as EUC-JP";
}

TEST(TextEncodingDetectorTest, Utf8DetectionShouldFail) {
  std::string utf8_bytes =
      "tnegirjji gosa gii beare s\xC3\xA1htt\xC3\xA1 \xC4\x8D\xC3"
      "\xA1llit artihkkaliid. Maid don s\xC3\xA1ht\xC3\xA1t dievasmah";
  TextEncoding encoding;
  bool result = DetectTextEncoding(base::as_byte_span(utf8_bytes), nullptr,
                                   NullUrl(), nullptr, &encoding);
  EXPECT_FALSE(result);
}

TEST(TextEncodingDetectorTest, RespectUtf8DetectionForFileResource) {
  std::string utf8_bytes =
      "tnegirjji gosa gii beare s\xC3\xA1htt\xC3\xA1 \xC4\x8D\xC3"
      "\xA1llit artihkkaliid. Maid don s\xC3\xA1ht\xC3\xA1t dievasmah";
  TextEncoding encoding;
  KURL file_url("file:///text.txt");
  bool result = DetectTextEncoding(base::as_byte_span(utf8_bytes), nullptr,
                                   file_url, nullptr, &encoding);
  EXPECT_TRUE(result);
}

// Embedders (e.g. Chrome for Android and Android WebView) register "content"
// as a local scheme. Detected UTF-8 must be honored for such local resources
// just like for file://, otherwise a BOM-less UTF-8 text file would be decoded
// with the locale default encoding (windows-949 for Korean) and garbled.
TEST(TextEncodingDetectorTest, RespectUtf8DetectionForRegisteredLocalScheme) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddLocalScheme("content");

  TextEncoding encoding;
  KURL content_url("content://com.example.provider/document/1.txt");
  bool result =
      DetectTextEncoding(base::as_byte_span(std::string_view(kUtf8KoreanText)),
                         nullptr, content_url, nullptr, &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(TextEncoding("UTF-8"), encoding);
}

// The local-resource exception follows url::GetLocalSchemes(); a scheme that
// is not registered as local keeps the web policy of rejecting detected UTF-8.
TEST(TextEncodingDetectorTest, RejectUtf8DetectionForNonLocalScheme) {
  url::ScopedSchemeRegistryForTests scoped_registry;

  TextEncoding encoding;
  KURL non_local_url("not-a-local-scheme://example/document/1.txt");
  bool result =
      DetectTextEncoding(base::as_byte_span(std::string_view(kUtf8KoreanText)),
                         nullptr, non_local_url, nullptr, &encoding);
  EXPECT_FALSE(result);
}

// The language hint is likewise applied to registered local schemes.
TEST(TextEncodingDetectorTest, LanguageHintHelpsEucJpForRegisteredLocalScheme) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddLocalScheme("content");

  std::string eucjp_bytes =
      "<TITLE>"
      "\xA5\xD1\xA5\xEF\xA1\xBC\xA5\xC1\xA5\xE3\xA1\xBC\xA5\xC8\xA1\xC3\xC5\xEA"
      "\xBB\xF1\xBE\xF0\xCA\xF3\xA4\xCE\xA5\xD5\xA5\xA3\xA5\xB9\xA5\xB3</"
      "TITLE>";
  TextEncoding encoding;
  KURL content_url("content://com.example.provider/document/1.html");
  bool result = DetectTextEncoding(base::as_byte_span(eucjp_bytes), nullptr,
                                   content_url, "ja", &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(TextEncoding("EUC-JP"), encoding)
      << "Language hint works for a registered local scheme";
}

}  // namespace blink
