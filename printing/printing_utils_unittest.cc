// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_utils.h"

#include <stddef.h>

#include <limits>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_hdc.h"
#include "printing/backend/printing_info_win.h"
#include "printing/backend/win_helper.h"
#include "printing/printing_test.h"
#include "ui/gfx/geometry/rect.h"
#endif

namespace printing {

namespace {

constexpr size_t kTestLength = 8;

#if BUILDFLAG(USE_CUPS) && !BUILDFLAG(IS_CHROMEOS_ASH)
constexpr gfx::Size kIsoA4Microns(210000, 297000);
constexpr gfx::Size kNaLetterMicrons(216000, 279000);
#endif

std::string Simplify(const std::string& title) {
  return base::UTF16ToUTF8(
      SimplifyDocumentTitleWithLength(base::UTF8ToUTF16(title), kTestLength));
}

std::string Format(const std::string& owner, const std::string& title) {
  return base::UTF16ToUTF8(FormatDocumentTitleWithOwnerAndLength(
      base::UTF8ToUTF16(owner), base::UTF8ToUTF16(title), kTestLength));
}

#if BUILDFLAG(IS_WIN)
// This test is automatically disabled if no printer is available.
class PrintingUtilsWinTest : public PrintingTest<testing::Test> {};
#endif  // BUILDFLAG(IS_WIN)

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

#if BUILDFLAG(USE_CUPS) && !BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif  // BUILDFLAG(USE_CUPS) && !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
TEST(PrintingUtilsTest, GetCenteredPageContentRect) {
  gfx::Rect page_content;

  // No centering.
  gfx::Size page_size = gfx::Size(1200, 1200);
  gfx::Rect page_content_rect = gfx::Rect(0, 0, 400, 1100);
  page_content = GetCenteredPageContentRect(gfx::Size(1000, 1000), page_size,
                                            page_content_rect);
  EXPECT_EQ(0, page_content.x());
  EXPECT_EQ(0, page_content.y());
  EXPECT_EQ(400, page_content.width());
  EXPECT_EQ(1100, page_content.height());

  // X centered.
  page_size = gfx::Size(500, 1200);
  page_content = GetCenteredPageContentRect(gfx::Size(1000, 1000), page_size,
                                            page_content_rect);
  EXPECT_EQ(250, page_content.x());
  EXPECT_EQ(0, page_content.y());
  EXPECT_EQ(400, page_content.width());
  EXPECT_EQ(1100, page_content.height());

  // Y centered.
  page_size = gfx::Size(1200, 500);
  page_content = GetCenteredPageContentRect(gfx::Size(1000, 1000), page_size,
                                            page_content_rect);
  EXPECT_EQ(0, page_content.x());
  EXPECT_EQ(250, page_content.y());
  EXPECT_EQ(400, page_content.width());
  EXPECT_EQ(1100, page_content.height());

  // Both X and Y centered.
  page_size = gfx::Size(500, 500),
  page_content = GetCenteredPageContentRect(gfx::Size(1000, 1000), page_size,
                                            page_content_rect);
  EXPECT_EQ(250, page_content.x());
  EXPECT_EQ(250, page_content.y());
  EXPECT_EQ(400, page_content.width());
  EXPECT_EQ(1100, page_content.height());
}

// Disabled - see crbug.com/1231528 for context.
TEST_F(PrintingUtilsWinTest, DISABLED_GetPrintableAreaDeviceUnits) {
  if (IsTestCaseDisabled()) {
    return;
  }

  std::wstring printer_name = GetDefaultPrinter();
  ScopedPrinterHandle printer;
  ASSERT_TRUE(printer.OpenPrinterWithName(printer_name.c_str()));

  const DEVMODE* dev_mode = nullptr;
  PrinterInfo2 info_2;
  if (info_2.Init(printer.Get())) {
    dev_mode = info_2.get()->pDevMode;
  }
  ASSERT_TRUE(dev_mode);

  base::win::ScopedCreateDC hdc(
      CreateDC(L"WINSPOOL", printer_name.c_str(), nullptr, dev_mode));
  ASSERT_TRUE(hdc.Get());

  // Check that getting printable area is successful and the resulting area is
  // non-empty.
  gfx::Rect output = GetPrintableAreaDeviceUnits(hdc.Get());
  EXPECT_FALSE(output.IsEmpty());
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace printing
