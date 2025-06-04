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

TEST(PrintBackendUtilsTest, GetDisplayName) {
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(GetDisplayName("HP Printer", "Detailed Printer Info"),
            "Detailed Printer Info");
  EXPECT_EQ(GetDisplayName("HP Printer", ""), "HP Printer");
#else
  EXPECT_EQ(GetDisplayName("HP Printer", "Detailed Printer Info"),
            "HP Printer");
  EXPECT_EQ(GetDisplayName("HP Printer", ""), "HP Printer");
#endif
}

TEST(PrintBackendUtilsTest, GetPrinterDescription) {
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(GetPrinterDescription("Driver Info", "Some Description"),
            "Driver Info");
  EXPECT_EQ(GetPrinterDescription("", "Some Description"), "");
#else
  EXPECT_EQ(GetPrinterDescription("Driver Info", "Some Description"),
            "Some Description");
  EXPECT_EQ(GetPrinterDescription("Driver Info", ""), "");
#endif
}

TEST(PrintBackendUtilsCupsTest, PrintableAreaFromMarginsA4) {
  // margins in PWG units (1 PWG unit = 1/100 mm = 10 um)
  int bottom = 100;
  int left = 200;
  int right = 300;
  int top = 400;
  gfx::Size size_um = {210000, 297000};
  gfx::Rect printable_area_um =
      PrintableAreaFromSizeAndPwgMargins(size_um, bottom, left, right, top);
  EXPECT_EQ(gfx::Rect(2000, 1000, 205000, 292000), printable_area_um);
}

TEST(PrintBackendUtilsCupsTest, MarginsPWGFromPrintableAreaA4) {
  static constexpr int kBottomUm = 1000;
  static constexpr int kLeftUm = 2000;
  static constexpr int kRightUm = 3000;
  static constexpr int kTopUm = 4000;

  int bottom_um = 0;
  int left_um = 0;
  int right_um = 0;
  int top_um = 0;
  MarginsMicronsFromSizeAndPrintableArea(
      {210000, 297000}, {2000, 1000, 205000, 292000}, &bottom_um, &left_um,
      &right_um, &top_um);

  // Verify margins in microns are correct.
  EXPECT_EQ(kBottomUm, bottom_um);
  EXPECT_EQ(kLeftUm, left_um);
  EXPECT_EQ(kRightUm, right_um);
  EXPECT_EQ(kTopUm, top_um);

  ASSERT_EQ(kMicronsPerPwgUnit, 10);
  // Verify margins in PWG units are correct.
  EXPECT_EQ(kBottomUm / kMicronsPerPwgUnit, MarginMicronsToPWG(bottom_um));
  EXPECT_EQ(kLeftUm / kMicronsPerPwgUnit, MarginMicronsToPWG(left_um));
  EXPECT_EQ(kRightUm / kMicronsPerPwgUnit, MarginMicronsToPWG(right_um));
  EXPECT_EQ(kTopUm / kMicronsPerPwgUnit, MarginMicronsToPWG(top_um));
}

#endif  // BUILDFLAG(USE_CUPS)

}  // namespace printing
