// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/text_encoding_detector.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {

TEST(TextEncodingDetectorTest, RespectIso2022Jp) {
  // ISO-2022-JP is the only 7-bit encoding defined in WHATWG standard.
  std::string iso2022jp =
      " \x1B"
      "$BKL3$F;F|K\\%O%`%U%!%$%?!<%:$,%=%U%H%P%s%/$H$N%W%l!<%*%U$r@)$7!\"";
  WTF::TextEncoding encoding;
  bool result = DetectTextEncoding(base::as_byte_span(iso2022jp), nullptr,
                                   NullURL(), nullptr, &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(WTF::TextEncoding("ISO-2022-JP"), encoding);
}

TEST(TextEncodingDetectorTest, Ignore7BitEncoding) {
  // 7-bit encodings except ISO-2022-JP are not supported by WHATWG.
  // They should be detected as plain text (US-ASCII).
  std::string hz_gb2312 =
      " ~{\x54\x42\x31\x7D\x37\x22\x55\x39\x35\x3D\x3D\x71~} abc";
  WTF::TextEncoding encoding;
  bool result = DetectTextEncoding(base::as_byte_span(hz_gb2312), nullptr,
                                   NullURL(), nullptr, &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(WTF::TextEncoding("US-ASCII"), encoding);
}

TEST(TextEncodingDetectorTest, NonWHATWGEncodingBecomesAscii) {
  std::string pseudo_jpg =
      "\xff\xd8\xff\xe0\x00\x10JFIF foo bar baz\xff\xe1\x00\xa5"
      "\x01\xd7\xff\x01\x57\x33\x44\x55\x66\x77\xed\xcb\xa9\x87"
      "\xff\xd7\xff\xe0\x00\x10JFIF foo bar baz\xff\xe1\x00\xa5"
      "\x87\x01\xd7\xff\x01\x57\x33\x44\x55\x66\x77\xed\xcb\xa9";
  WTF::TextEncoding encoding;
  bool result = DetectTextEncoding(base::as_byte_span(pseudo_jpg), nullptr,
                                   NullURL(), nullptr, &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(WTF::TextEncoding("US-ASCII"), encoding);
}

TEST(TextEncodingDetectorTest, UrlHintHelpsEUCJP) {
  std::string eucjp_bytes =
      "<TITLE>"
      "\xA5\xD1\xA5\xEF\xA1\xBC\xA5\xC1\xA5\xE3\xA1\xBC\xA5\xC8\xA1\xC3\xC5\xEA"
      "\xBB\xF1\xBE\xF0\xCA\xF3\xA4\xCE\xA5\xD5\xA5\xA3\xA5\xB9\xA5\xB3</"
      "TITLE>";
  WTF::TextEncoding encoding;
  bool result = DetectTextEncoding(base::as_byte_span(eucjp_bytes), nullptr,
                                   NullURL(), nullptr, &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(WTF::TextEncoding("GBK"), encoding)
      << "Without language hint, it's detected as GBK";

  KURL url_jp_domain("http://example.co.jp/");
  result = DetectTextEncoding(base::as_byte_span(eucjp_bytes), nullptr,
                              url_jp_domain, nullptr, &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(WTF::TextEncoding("EUC-JP"), encoding)
      << "With URL hint including '.jp', it's detected as EUC-JP";
}

TEST(TextEncodingDetectorTest, LanguageHintHelpsEUCJP) {
  std::string eucjp_bytes =
      "<TITLE>"
      "\xA5\xD1\xA5\xEF\xA1\xBC\xA5\xC1\xA5\xE3\xA1\xBC\xA5\xC8\xA1\xC3\xC5\xEA"
      "\xBB\xF1\xBE\xF0\xCA\xF3\xA4\xCE\xA5\xD5\xA5\xA3\xA5\xB9\xA5\xB3</"
      "TITLE>";
  WTF::TextEncoding encoding;
  bool result = DetectTextEncoding(base::as_byte_span(eucjp_bytes), nullptr,
                                   NullURL(), nullptr, &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(WTF::TextEncoding("GBK"), encoding)
      << "Without language hint, it's detected as GBK";

  KURL url("http://example.com/");
  result = DetectTextEncoding(base::as_byte_span(eucjp_bytes), nullptr, url,
                              "ja", &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(WTF::TextEncoding("GBK"), encoding)
      << "Language hint doesn't help for normal URL. Should be detected as GBK";

  KURL file_url("file:///text.txt");
  result = DetectTextEncoding(base::as_byte_span(eucjp_bytes), nullptr,
                              file_url, "ja", &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(WTF::TextEncoding("EUC-JP"), encoding)
      << "Language hint works for file resource. Should be detected as EUC-JP";
}

TEST(TextEncodingDetectorTest, UTF8DetectionShouldFail) {
  std::string utf8_bytes =
      "tnegirjji gosa gii beare s\xC3\xA1htt\xC3\xA1 \xC4\x8D\xC3"
      "\xA1llit artihkkaliid. Maid don s\xC3\xA1ht\xC3\xA1t dievasmah";
  WTF::TextEncoding encoding;
  bool result = DetectTextEncoding(base::as_byte_span(utf8_bytes), nullptr,
                                   NullURL(), nullptr, &encoding);
  EXPECT_FALSE(result);
}

TEST(TextEncodingDetectorTest, RespectUTF8DetectionForFileResource) {
  std::string utf8_bytes =
      "tnegirjji gosa gii beare s\xC3\xA1htt\xC3\xA1 \xC4\x8D\xC3"
      "\xA1llit artihkkaliid. Maid don s\xC3\xA1ht\xC3\xA1t dievasmah";
  WTF::TextEncoding encoding;
  KURL file_url("file:///text.txt");
  bool result = DetectTextEncoding(base::as_byte_span(utf8_bytes), nullptr,
                                   file_url, nullptr, &encoding);
  EXPECT_TRUE(result);
}

}  // namespace blink
