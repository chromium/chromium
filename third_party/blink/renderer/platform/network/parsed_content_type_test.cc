// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/network/parsed_content_type.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

using Mode = ParsedContentType::Mode;

bool IsValidContentType(const String& input, Mode mode = Mode::kNormal) {
  return ParsedContentType(input, mode).IsValid();
}

TEST(ParsedContentTypeTest, MimeTypeWithoutCharset) {
  ParsedContentType t("text/plain");

  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ("text/plain", t.MimeType());
  EXPECT_EQ(String(), t.Charset());
}

TEST(ParsedContentTypeTest, MimeTypeWithCharSet) {
  ParsedContentType t("text /  plain  ;  x=y; charset = utf-8 ");

  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ("text/plain", t.MimeType());
  EXPECT_EQ("utf-8", t.Charset());
}

TEST(ParsedContentTypeTest, MimeTypeWithQuotedCharSet) {
  ParsedContentType t("text/plain; charset=\"x=y;y=\\\"\\pz; ;;\"");

  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ("text/plain", t.MimeType());
  EXPECT_EQ("x=y;y=\"pz; ;;", t.Charset());
}

TEST(ParsedContentTypeTest, InvalidMimeTypeWithoutCharset) {
  ParsedContentType t(" ");

  EXPECT_FALSE(t.IsValid());
  EXPECT_EQ(String(), t.MimeType());
  EXPECT_EQ(String(), t.Charset());
}

TEST(ParsedContentTypeTest, InvalidMimeTypeWithCharset) {
  ParsedContentType t("text/plain; charset;");

  EXPECT_FALSE(t.IsValid());
  EXPECT_EQ("text/plain", t.MimeType());
  EXPECT_EQ(String(), t.Charset());
}

TEST(ParsedContentTypeTest, CaseInsensitiveCharset) {
  ParsedContentType t("text/plain; cHaRsEt=utf-8");

  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ("text/plain", t.MimeType());
  EXPECT_EQ("utf-8", t.Charset());
}

TEST(ParsedContentTypeTest, Validity) {
  EXPECT_TRUE(IsValidContentType("text/plain"));
  EXPECT_TRUE(IsValidContentType("text/plain; charset=utf-8"));
  EXPECT_TRUE(IsValidContentType(" text/plain ;charset=utf-8  "));
  EXPECT_TRUE(IsValidContentType("  text/plain  "));
  EXPECT_TRUE(IsValidContentType("unknown/unknown"));
  EXPECT_TRUE(IsValidContentType("unknown/unknown; charset=unknown"));

  EXPECT_FALSE(IsValidContentType("A"));
  EXPECT_FALSE(IsValidContentType("text/plain\r"));
  EXPECT_FALSE(IsValidContentType("text/plain\n"));
  EXPECT_FALSE(IsValidContentType("text/plain charset=utf-8"));
  EXPECT_FALSE(IsValidContentType("text/plain;charset=utf-8;"));
  EXPECT_FALSE(IsValidContentType(""));
  EXPECT_FALSE(IsValidContentType("   "));
  EXPECT_FALSE(IsValidContentType("\"x\""));
  EXPECT_FALSE(IsValidContentType("\"x\"/\"y\""));
  EXPECT_FALSE(IsValidContentType("\"x\"/y"));
  EXPECT_FALSE(IsValidContentType("x/\"y\""));
  EXPECT_FALSE(IsValidContentType("text/plain;"));
  EXPECT_FALSE(IsValidContentType("text/plain;  "));
  EXPECT_FALSE(IsValidContentType("text/plain; charset"));
  EXPECT_FALSE(IsValidContentType("text/plain; charset;"));
}

}  // namespace

}  // namespace blink
