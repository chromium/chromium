// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_utils.h"

#include <stddef.h>

#include <limits>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace {

constexpr size_t kTestLength = 8;
constexpr gfx::Size kIsoA4Microns(210000, 297000);
constexpr gfx::Size kNaLetterMicrons(216000, 279000);

std::string Simplify(const std::string& title) {
  return base::UTF16ToUTF8(
      SimplifyDocumentTitleWithLength(base::UTF8ToUTF16(title), kTestLength));
}

std::string Format(const std::string& owner, const std::string& title) {
  return base::UTF16ToUTF8(FormatDocumentTitleWithOwnerAndLength(
      base::UTF8ToUTF16(owner), base::UTF8ToUTF16(title), kTestLength));
}

}  // namespace

TEST(PrintingUtilsTest, SimplifyDocumentTitle) {
  EXPECT_EQ("", Simplify(""));
  EXPECT_EQ("abcdefgh", Simplify("abcdefgh"));
  EXPECT_EQ("abc...ij", Simplify("abcdefghij"));
  EXPECT_EQ("Controls", Simplify("C\ron\nt\15rols"));
  EXPECT_EQ("C__foo_", Simplify("C:\\foo\\"));
  EXPECT_EQ("C__foo_", Simplify("C:/foo/"));
  EXPECT_EQ("a_b_c", Simplify("a<b\"c"));
  EXPECT_EQ("d_e_f_", Simplify("d*e?f~"));
  EXPECT_EQ("", Simplify("\n\r\n\r\t\r"));
}

TEST(PrintingUtilsTest, FormatDocumentTitleWithOwner) {
  EXPECT_EQ(": ", Format("", ""));
  EXPECT_EQ("abc: ", Format("abc", ""));
  EXPECT_EQ(": 123", Format("", "123"));
  EXPECT_EQ("abc: 123", Format("abc", "123"));
  EXPECT_EQ("abc: 0.9", Format("abc", "0123456789"));
  EXPECT_EQ("ab...j: ", Format("abcdefghij", "123"));
  EXPECT_EQ("xyz: _.o", Format("xyz", "\\f\\oo"));
  EXPECT_EQ("ab...j: ", Format("abcdefghij", "0123456789"));
}

TEST(PrintingUtilsTest, GetDefaultPaperSizeFromLocaleMicrons) {
  // Valid locales
  EXPECT_EQ(kNaLetterMicrons, GetDefaultPaperSizeFromLocaleMicrons("en-US"));
  EXPECT_EQ(kNaLetterMicrons, GetDefaultPaperSizeFromLocaleMicrons("en_US"));
  EXPECT_EQ(kNaLetterMicrons, GetDefaultPaperSizeFromLocaleMicrons("fr-CA"));
  EXPECT_EQ(kNaLetterMicrons, GetDefaultPaperSizeFromLocaleMicrons("es-CL"));
  EXPECT_EQ(kIsoA4Microns, GetDefaultPaperSizeFromLocaleMicrons("en_UK"));
  EXPECT_EQ(kIsoA4Microns, GetDefaultPaperSizeFromLocaleMicrons("fa-IR"));

  // Empty locale
  EXPECT_EQ(kIsoA4Microns, GetDefaultPaperSizeFromLocaleMicrons(""));

  // Non-existing locale
  EXPECT_EQ(kIsoA4Microns,
            GetDefaultPaperSizeFromLocaleMicrons("locale-does-not-exist"));
}

TEST(PrintingUtilsTest, SizesEqualWithinEpsilon) {
  constexpr int kMaxInt = std::numeric_limits<int>::max();

  // Large sizes
  EXPECT_TRUE(SizesEqualWithinEpsilon(gfx::Size(kMaxInt, kMaxInt),
                                      gfx::Size(kMaxInt - 1, kMaxInt - 1), 1));
  EXPECT_FALSE(SizesEqualWithinEpsilon(gfx::Size(kMaxInt, kMaxInt),
                                       gfx::Size(kMaxInt - 1, kMaxInt - 2), 1));
  EXPECT_TRUE(SizesEqualWithinEpsilon(gfx::Size(kMaxInt, kMaxInt),
                                      gfx::Size(0, 0), kMaxInt));
  EXPECT_FALSE(SizesEqualWithinEpsilon(gfx::Size(kMaxInt, kMaxInt),
                                       gfx::Size(0, 0), kMaxInt - 1));

  // Empty sizes
  EXPECT_TRUE(SizesEqualWithinEpsilon(gfx::Size(0, 0), gfx::Size(0, 0), 0));
  EXPECT_TRUE(SizesEqualWithinEpsilon(gfx::Size(1, 0), gfx::Size(0, 2), 0));
  EXPECT_TRUE(SizesEqualWithinEpsilon(gfx::Size(1, -2), gfx::Size(-1, 2), 0));

  // Common paper sizes
  EXPECT_FALSE(SizesEqualWithinEpsilon(kNaLetterMicrons, kIsoA4Microns, 1000));
  EXPECT_TRUE(SizesEqualWithinEpsilon(kNaLetterMicrons,
                                      gfx::Size(215900, 279400), 500));
  EXPECT_TRUE(
      SizesEqualWithinEpsilon(kIsoA4Microns, gfx::Size(210500, 296500), 500));
}

}  // namespace printing
