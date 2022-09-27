// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/network/parsed_content_disposition.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

using Mode = ParsedContentDisposition::Mode;

bool IsValidContentDisposition(const String& input, Mode mode = Mode::kNormal) {
  return ParsedContentDisposition(input, mode).IsValid();
}

TEST(ParsedContentDispositionTest, TypeWithoutFilename) {
  ParsedContentDisposition t("attachment");

  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ("attachment", t.Type());
  EXPECT_EQ(String(), t.Filename());
}

TEST(ParsedContentDispositionTest, TypeWithFilename) {
  ParsedContentDisposition t("  attachment  ;  x=y; filename = file1 ");

  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ("attachment", t.Type());
  EXPECT_EQ("file1", t.Filename());
}

TEST(ParsedContentDispositionTest, TypeWithQuotedFilename) {
  ParsedContentDisposition t("attachment; filename=\"x=y;y=\\\"\\pz; ;;\"");

  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ("attachment", t.Type());
  EXPECT_EQ("x=y;y=\"pz; ;;", t.Filename());
}

TEST(ParsedContentDispositionTest, InvalidTypeWithoutFilename) {
  ParsedContentDisposition t(" ");

  EXPECT_FALSE(t.IsValid());
  EXPECT_EQ(String(), t.Type());
  EXPECT_EQ(String(), t.Filename());
}

TEST(ParsedContentDispositionTest, InvalidTypeWithFilename) {
  ParsedContentDisposition t("/attachment; filename=file1;");

  EXPECT_FALSE(t.IsValid());
  EXPECT_EQ(String(), t.Type());
  EXPECT_EQ(String(), t.Filename());
}

TEST(ParsedContentDispositionTest, CaseInsensitiveFilename) {
  ParsedContentDisposition t("attachment; fIlEnAmE=file1");

  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ("attachment", t.Type());
  EXPECT_EQ("file1", t.Filename());
}

TEST(ParsedContentDispositionTest, Validity) {
  EXPECT_TRUE(IsValidContentDisposition("attachment"));
  EXPECT_TRUE(IsValidContentDisposition("attachment; filename=file1"));
  EXPECT_TRUE(
      IsValidContentDisposition("attachment; filename*=UTF-8'en'file1"));
  EXPECT_TRUE(IsValidContentDisposition(" attachment ;filename=file1 "));
  EXPECT_TRUE(IsValidContentDisposition("  attachment  "));
  EXPECT_TRUE(IsValidContentDisposition("unknown-unknown"));
  EXPECT_TRUE(IsValidContentDisposition("unknown-unknown; unknown=unknown"));

  EXPECT_FALSE(IsValidContentDisposition("A/B"));
  EXPECT_FALSE(IsValidContentDisposition("attachment\r"));
  EXPECT_FALSE(IsValidContentDisposition("attachment\n"));
  EXPECT_FALSE(IsValidContentDisposition("attachment filename=file1"));
  EXPECT_FALSE(IsValidContentDisposition("attachment;filename=file1;"));
  EXPECT_FALSE(IsValidContentDisposition(""));
  EXPECT_FALSE(IsValidContentDisposition("   "));
  EXPECT_FALSE(IsValidContentDisposition("\"x\""));
  EXPECT_FALSE(IsValidContentDisposition("attachment;"));
  EXPECT_FALSE(IsValidContentDisposition("attachment;  "));
  EXPECT_FALSE(IsValidContentDisposition("attachment; filename"));
  EXPECT_FALSE(IsValidContentDisposition("attachment; filename;"));
}

}  // namespace

}  // namespace blink
