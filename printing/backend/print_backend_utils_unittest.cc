// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend_utils.h"

#include <map>
#include <memory>

#include "printing/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(USE_CUPS)
#include "printing/backend/cups_printer.h"
#include "printing/backend/print_backend.h"
#include "printing/units.h"
#include "ui/gfx/geometry/rect.h"
#endif  // BUILDFLAG(USE_CUPS)

namespace printing {

TEST(PrintBackendUtilsTest, ParsePaperSizeA4) {
  gfx::Size size = ParsePaperSize("iso_a4_210x297mm");
  EXPECT_EQ(gfx::Size(210000, 297000), size);
}

TEST(PrintBackendUtilsTest, ParsePaperSizeNaLetter) {
  gfx::Size size = ParsePaperSize("na_letter_8.5x11in");
  EXPECT_EQ(gfx::Size(215900, 279400), size);
}

TEST(PrintBackendUtilsTest, ParsePaperSizeNaIndex4x6) {
  // Note that "na_index-4x6_4x6in" has a dimension within the media name. Test
  // that parsing is not affected.
  gfx::Size size = ParsePaperSize("na_index-4x6_4x6in");
  EXPECT_EQ(gfx::Size(101600, 152400), size);
}

TEST(PrintBackendUtilsTest, ParsePaperSizeNaNumber10) {
  // Test that a paper size with a fractional dimension is not affected by
  // rounding errors.
  gfx::Size size = ParsePaperSize("na_number-10_4.125x9.5in");
  EXPECT_EQ(gfx::Size(104775, 241300), size);
}

TEST(PrintBackendUtilsTest, ParsePaperSizeBadUnit) {
  gfx::Size size = ParsePaperSize("bad_unit_666x666bad");
  EXPECT_TRUE(size.IsEmpty());
}

TEST(PrintBackendUtilsTest, ParsePaperSizeBadOneDimension) {
  gfx::Size size = ParsePaperSize("bad_one_dimension_666mm");
  EXPECT_TRUE(size.IsEmpty());
}

#if BUILDFLAG(USE_CUPS)

TEST(PrintBackendUtilsCupsTest, ParsePaperA4) {
  constexpr CupsPrinter::CupsMediaMargins kMargins = {500, 500, 500, 500};
  PrinterSemanticCapsAndDefaults::Paper paper =
      ParsePaper("iso_a4_210x297mm", kMargins);
  EXPECT_EQ(gfx::Size(210000, 297000), paper.size_um);
  EXPECT_EQ("iso_a4_210x297mm", paper.vendor_id);
  EXPECT_EQ("iso a4", paper.display_name);
  EXPECT_EQ(gfx::Rect(5000, 5000, 200000, 287000), paper.printable_area_um);
}

TEST(PrintBackendUtilsCupsTest, ParsePaperNaLetter) {
  constexpr CupsPrinter::CupsMediaMargins kMargins = {500, 500, 500, 500};
  PrinterSemanticCapsAndDefaults::Paper paper =
      ParsePaper("na_letter_8.5x11in", kMargins);
  EXPECT_EQ(gfx::Size(215900, 279400), paper.size_um);
  EXPECT_EQ("na_letter_8.5x11in", paper.vendor_id);
  EXPECT_EQ("na letter", paper.display_name);
  EXPECT_EQ(gfx::Rect(5000, 5000, 205900, 269400), paper.printable_area_um);
}

TEST(PrintBackendUtilsCupsTest, ParsePaperNaIndex4x6) {
  // Note that "na_index-4x6_4x6in" has a dimension within the media name. Test
  // that parsing is not affected.
  constexpr CupsPrinter::CupsMediaMargins kMargins = {500, 500, 500, 500};
  PrinterSemanticCapsAndDefaults::Paper paper =
      ParsePaper("na_index-4x6_4x6in", kMargins);
  EXPECT_EQ(gfx::Size(101600, 152400), paper.size_um);
  EXPECT_EQ("na_index-4x6_4x6in", paper.vendor_id);
  EXPECT_EQ("na index-4x6", paper.display_name);
  EXPECT_EQ(gfx::Rect(5000, 5000, 91600, 142400), paper.printable_area_um);
}

TEST(PrintBackendUtilsCupsTest, ParsePaperNaNumber10) {
  // Test that a paper size with a fractional dimension is not affected by
  // rounding errors.
  constexpr CupsPrinter::CupsMediaMargins kMargins = {1000, 1000, 1000, 1000};
  PrinterSemanticCapsAndDefaults::Paper paper =
      ParsePaper("na_number-10_4.125x9.5in", kMargins);
  EXPECT_EQ(gfx::Size(104775, 241300), paper.size_um);
  EXPECT_EQ("na_number-10_4.125x9.5in", paper.vendor_id);
  EXPECT_EQ("na number-10", paper.display_name);
  EXPECT_EQ(gfx::Rect(10000, 10000, 84775, 221300), paper.printable_area_um);
}

TEST(PrintBackendUtilsCupsTest, ParsePaperBadUnit) {
  PrinterSemanticCapsAndDefaults::Paper paper_bad =
      ParsePaper("bad_unit_666x666bad", CupsPrinter::CupsMediaMargins());
  EXPECT_EQ(PrinterSemanticCapsAndDefaults::Paper(), paper_bad);
}

TEST(PrintBackendUtilsCupsTest, ParsePaperBadOneDimension) {
  PrinterSemanticCapsAndDefaults::Paper paper_bad =
      ParsePaper("bad_one_dimension_666mm", CupsPrinter::CupsMediaMargins());
  EXPECT_EQ(PrinterSemanticCapsAndDefaults::Paper(), paper_bad);
}

TEST(PrintBackendUtilsCupsTest, ParsePaperOutOfBoundsMargins) {
  // Given invalid margins, the printable area cannot be calculated correctly.
  // The printable area should be set to the paper size as default.
  constexpr CupsPrinter::CupsMediaMargins kMargins = {100, 100, 300000, 100};
  PrinterSemanticCapsAndDefaults::Paper paper =
      ParsePaper("iso_a4_210x297mm", kMargins);
  EXPECT_EQ(gfx::Size(210000, 297000), paper.size_um);
  EXPECT_EQ("iso_a4_210x297mm", paper.vendor_id);
  EXPECT_EQ("iso a4", paper.display_name);
  EXPECT_EQ(gfx::Rect(0, 0, 210000, 297000), paper.printable_area_um);
}

TEST(PrintBackendUtilsCupsTest, ParsePaperEmptyPrintableArea) {
  // If the printable area is empty, the printable area should be set to the
  // paper size.
  constexpr CupsPrinter::CupsMediaMargins kMargins = {29700, 0, 0, 0};
  PrinterSemanticCapsAndDefaults::Paper paper =
      ParsePaper("iso_a4_210x297mm", kMargins);
  EXPECT_EQ(gfx::Size(210000, 297000), paper.size_um);
  EXPECT_EQ("iso_a4_210x297mm", paper.vendor_id);
  EXPECT_EQ("iso a4", paper.display_name);
  EXPECT_EQ(gfx::Rect(0, 0, 210000, 297000), paper.printable_area_um);
}

TEST(PrintBackendUtilsCupsTest, ParsePaperEmptySizeWithPrintableArea) {
  // If the paper size is empty, the Paper should be invalid, even when provided
  // a printable area.
  constexpr CupsPrinter::CupsMediaMargins kMargins = {1000, 1000, 1000, 1000};
  PrinterSemanticCapsAndDefaults::Paper paper_bad =
      ParsePaper("bad_unit_666x666bad", kMargins);
  EXPECT_EQ(PrinterSemanticCapsAndDefaults::Paper(), paper_bad);
}

#endif  // BUILDFLAG(USE_CUPS)

}  // namespace printing
