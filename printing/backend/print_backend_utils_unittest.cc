// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend_utils.h"

#include "printing/backend/print_backend.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

TEST(PrintBackendUtilsTest, ParsePaperA4) {
  PrinterSemanticCapsAndDefaults::Paper paper = ParsePaper("iso_a4_210x297mm");
  EXPECT_EQ(gfx::Size(210000, 297000), paper.size_um);
  EXPECT_EQ("iso_a4_210x297mm", paper.vendor_id);
  EXPECT_EQ("iso a4", paper.display_name);
}

TEST(PrintBackendUtilsTest, ParsePaperNaLetter) {
  PrinterSemanticCapsAndDefaults::Paper paper =
      ParsePaper("na_letter_8.5x11in");
  EXPECT_EQ(gfx::Size(215900, 279400), paper.size_um);
  EXPECT_EQ("na_letter_8.5x11in", paper.vendor_id);
  EXPECT_EQ("na letter", paper.display_name);
}

TEST(PrintBackendUtilsTest, ParsePaperNaIndex4x6) {
  // Note that "na_index-4x6_4x6in" has a dimension within the media name. Test
  // that parsing is not affected.
  PrinterSemanticCapsAndDefaults::Paper paper =
      ParsePaper("na_index-4x6_4x6in");
  EXPECT_EQ(gfx::Size(101600, 152400), paper.size_um);
  EXPECT_EQ("na_index-4x6_4x6in", paper.vendor_id);
  EXPECT_EQ("na index-4x6", paper.display_name);
}

TEST(PrintBackendUtilsTest, ParsePaperNaNumber10) {
  // Test that a paper size with a fractional dimension is not affected by
  // rounding errors.
  PrinterSemanticCapsAndDefaults::Paper paper =
      ParsePaper("na_number-10_4.125x9.5in");
  EXPECT_EQ(gfx::Size(104775, 241300), paper.size_um);
  EXPECT_EQ("na_number-10_4.125x9.5in", paper.vendor_id);
  EXPECT_EQ("na number-10", paper.display_name);
}

TEST(PrintBackendUtilsTest, ParsePaperBadUnit) {
  PrinterSemanticCapsAndDefaults::Paper paper_bad =
      ParsePaper("bad_unit_666x666bad");
  EXPECT_TRUE(paper_bad.size_um.IsEmpty());
  EXPECT_EQ("bad_unit_666x666bad", paper_bad.vendor_id);
  EXPECT_EQ("bad unit", paper_bad.display_name);
}

TEST(PrintBackendUtilsTest, ParsePaperBadOneDimension) {
  PrinterSemanticCapsAndDefaults::Paper paper_bad =
      ParsePaper("bad_one_dimension_666mm");
  EXPECT_TRUE(paper_bad.size_um.IsEmpty());
  EXPECT_EQ("bad_one_dimension_666mm", paper_bad.vendor_id);
  EXPECT_EQ("bad one dimension", paper_bad.display_name);
}

}  // namespace printing
