// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings.h"

#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/units.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

TEST(PrintSettingsTest, ColorModeToColorModel) {
  for (int mode = static_cast<int>(mojom::ColorModel::kUnknownColorModel);
       mode <= static_cast<int>(mojom::ColorModel::kMaxValue); ++mode) {
    EXPECT_EQ(ColorModeToColorModel(mode),
              static_cast<mojom::ColorModel>(mode));
  }

  // Check edge cases.
  EXPECT_EQ(ColorModeToColorModel(
                static_cast<int>(mojom::ColorModel::kUnknownColorModel) - 1),
            mojom::ColorModel::kUnknownColorModel);
  EXPECT_EQ(
      ColorModeToColorModel(static_cast<int>(mojom::ColorModel::kMaxValue) + 1),
      mojom::ColorModel::kUnknownColorModel);
}

TEST(PrintSettingsTest, IsColorModelSelected) {
  for (int model = static_cast<int>(mojom::ColorModel::kUnknownColorModel) + 1;
       model <= static_cast<int>(mojom::ColorModel::kMaxValue); ++model) {
    EXPECT_TRUE(IsColorModelSelected(static_cast<mojom::ColorModel>(model))
                    .has_value());
  }
}

TEST(PrintSettingsDeathTest, IsColorModelSelectedEdges) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_NOTREACHED_DEATH(
      IsColorModelSelected(mojom::ColorModel::kUnknownColorModel));
}
#if BUILDFLAG(USE_CUPS)
TEST(PrintSettingsTest, GetColorModelForModel) {
  std::string color_setting_name;
  std::string color_value;
  for (int model = static_cast<int>(mojom::ColorModel::kUnknownColorModel);
       model <= static_cast<int>(mojom::ColorModel::kMaxValue); ++model) {
    GetColorModelForModel(static_cast<mojom::ColorModel>(model),
                          &color_setting_name, &color_value);
    EXPECT_FALSE(color_setting_name.empty());
    EXPECT_FALSE(color_value.empty());
    color_setting_name.clear();
    color_value.clear();
  }
}
#endif  // BUILDFLAG(USE_CUPS)

#if BUILDFLAG(USE_CUPS_IPP)
TEST(PrintSettingsTest, GetIppColorModelForModel) {
  for (int model = static_cast<int>(mojom::ColorModel::kUnknownColorModel);
       model <= static_cast<int>(mojom::ColorModel::kMaxValue); ++model) {
    EXPECT_FALSE(GetIppColorModelForModel(static_cast<mojom::ColorModel>(model))
                     .empty());
  }
}
#endif  // BUILDFLAG(USE_CUPS_IPP)

TEST(PrintSettingsTest, SetPrinterPrintableArea) {
  static constexpr gfx::Size kPhysicalSizeDeviceUnits(600, 800);
  static constexpr gfx::Rect kPrintableAreaDeviceUnits(50, 50, 500, 700);

  struct TestCase {
    int dpi;
    PageMargins margins;
    mojom::MarginType expected_margin_type;
  } static const kTestCases[] = {
      {
          300,
          PageMargins(),
          mojom::MarginType::kDefaultMargins,
      },
      {250, PageMargins(0, 0, 10000, 10000, 5000, 5000),
       mojom::MarginType::kCustomMargins},
      {426, PageMargins(0, 0, 20000, 20000, 25400, 25400),
       mojom::MarginType::kCustomMargins},
      {300, PageMargins(0, 0, 10000, 20000, 5000, 15000),
       mojom::MarginType::kCustomMargins}};

  for (const auto& test_case : kTestCases) {
    PrintSettings settings;
    settings.set_dpi(test_case.dpi);

    if (test_case.expected_margin_type == mojom::MarginType::kCustomMargins) {
      settings.SetCustomMargins(test_case.margins);
    }

    settings.SetPrinterPrintableArea(kPhysicalSizeDeviceUnits,
                                     kPrintableAreaDeviceUnits,
                                     /*landscape_needs_flip=*/false);

    EXPECT_EQ(test_case.expected_margin_type, settings.margin_type());
    EXPECT_EQ(test_case.margins.top,
              settings.requested_custom_margins_in_microns().top);
    EXPECT_EQ(test_case.margins.bottom,
              settings.requested_custom_margins_in_microns().bottom);
    EXPECT_EQ(test_case.margins.left,
              settings.requested_custom_margins_in_microns().left);
    EXPECT_EQ(test_case.margins.right,
              settings.requested_custom_margins_in_microns().right);

    const PageSetup& page_setup = settings.page_setup_device_units();

    if (test_case.expected_margin_type == mojom::MarginType::kDefaultMargins) {
#if BUILDFLAG(IS_MAC)
      EXPECT_EQ(PageMargins(50, 50, 50, 50, 50, 50),
                page_setup.effective_margins());
#else
      EXPECT_EQ(PageMargins(50, 50, 118, 118, 118, 118),
                page_setup.effective_margins());
#endif
    } else if (test_case.expected_margin_type ==
               mojom::MarginType::kCustomMargins) {
      const int device_units_per_inch = settings.device_units_per_inch();
      PageMargins expected_custom_margins;
      expected_custom_margins.header = 0;
      expected_custom_margins.footer = 0;
      expected_custom_margins.top = ConvertUnit(
          test_case.margins.top, kMicronsPerInch, device_units_per_inch);
      expected_custom_margins.bottom = ConvertUnit(
          test_case.margins.bottom, kMicronsPerInch, device_units_per_inch);
      expected_custom_margins.left = ConvertUnit(
          test_case.margins.left, kMicronsPerInch, device_units_per_inch);
      expected_custom_margins.right = ConvertUnit(
          test_case.margins.right, kMicronsPerInch, device_units_per_inch);

      EXPECT_EQ(expected_custom_margins, page_setup.effective_margins());
    }
  }
}

}  // namespace printing
