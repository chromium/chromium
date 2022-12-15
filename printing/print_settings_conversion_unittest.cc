// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings_conversion.h"

#include "base/containers/contains.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "printing/buildflags/buildflags.h"
#include "printing/print_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

const char kPrinterSettings[] = R"({
  "headerFooterEnabled": true,
  "title": "Test Doc",
  "url": "http://localhost/",
  "shouldPrintBackgrounds": false,
  "shouldPrintSelectionOnly": false,
  "mediaSize": {
    "height_microns": 297000,
    "width_microns": 210000
  },
  "marginsType": 0,
  "pageRange": [{
    "from": 1,
    "to": 1
  }],
  "collate": false,
  "copies": 1,
  "color": 2,
  "duplex": 0,
  "landscape": false,
  "deviceName": "printer",
  "scaleFactor": 100,
  "rasterizePDF": false,
  "rasterizePdfDpi": 150,
  "pagesPerSheet": 1,
  "dpiHorizontal": 300,
  "dpiVertical": 300,
  "previewModifiable": true,
  "sendUserInfo": true,
  "username": "username@domain.net",
  "chromeos-access-oauth-token": "this is an OAuth access token",
  "pinValue": "0000"
})";

const char kPrinterSettingsWithImageableArea[] = R"({
  "headerFooterEnabled": false,
  "title": "Test Doc",
  "url": "http://localhost/",
  "shouldPrintBackgrounds": false,
  "shouldPrintSelectionOnly": false,
  "mediaSize": {
    "height_microns": 297000,
    "imageable_area_bottom_microns": 1000,
    "imageable_area_left_microns": 0,
    "imageable_area_right_microns": 180000,
    "imageable_area_top_microns": 297000,
    "width_microns": 210000
  },
  "collate": false,
  "copies": 1,
  "color": 2,
  "duplex": 0,
  "landscape": false,
  "deviceName": "printer",
  "scaleFactor": 100,
  "rasterizePDF": false,
  "pagesPerSheet": 1,
  "dpiHorizontal": 300,
  "dpiVertical": 300,
})";

}  // namespace

TEST(PrintSettingsConversionTest, InvalidSettings) {
  base::Value value = base::test::ParseJson("{}");
  ASSERT_TRUE(value.is_dict());
  EXPECT_FALSE(PrintSettingsFromJobSettings(value.GetDict()));
}

TEST(PrintSettingsConversionTest, Conversion) {
  base::Value value = base::test::ParseJson(kPrinterSettings);
  ASSERT_TRUE(value.is_dict());
  auto& dict = value.GetDict();
  std::unique_ptr<PrintSettings> settings = PrintSettingsFromJobSettings(dict);
  ASSERT_TRUE(settings);
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(settings->send_user_info());
  EXPECT_EQ("username@domain.net", settings->username());
  EXPECT_EQ("this is an OAuth access token", settings->oauth_token());
  EXPECT_EQ("0000", settings->pin_value());
#endif
  EXPECT_EQ(settings->dpi_horizontal(), 300);
  EXPECT_EQ(settings->dpi_vertical(), 300);

  dict.Set("dpiVertical", 600);
  settings = PrintSettingsFromJobSettings(dict);
  ASSERT_TRUE(settings);
  EXPECT_EQ(settings->rasterize_pdf_dpi(), 150);
  EXPECT_EQ(settings->dpi_horizontal(), 300);
  EXPECT_EQ(settings->dpi_vertical(), 600);

  EXPECT_TRUE(dict.Remove("dpiVertical"));
  settings = PrintSettingsFromJobSettings(dict);
  EXPECT_FALSE(settings);
}

TEST(PrintSettingsConversionTest, WithValidImageableArea) {
#if BUILDFLAG(IS_MAC)
  static constexpr gfx::Size kExpectedSize{595, 842};
  static constexpr gfx::Rect kExpectedPrintableArea{0, 0, 510, 839};
#else
  static constexpr gfx::Size kExpectedSize{2480, 3508};
  static constexpr gfx::Rect kExpectedPrintableArea{0, 0, 2126, 3496};
#endif

  base::Value value = base::test::ParseJson(kPrinterSettingsWithImageableArea);
  ASSERT_TRUE(value.is_dict());
  auto& dict = value.GetDict();
  std::unique_ptr<PrintSettings> settings = PrintSettingsFromJobSettings(dict);
  ASSERT_TRUE(settings);
  EXPECT_EQ(settings->dpi_horizontal(), 300);
  EXPECT_EQ(settings->dpi_vertical(), 300);
  EXPECT_EQ(settings->page_setup_device_units().physical_size(), kExpectedSize);
  EXPECT_EQ(settings->page_setup_device_units().printable_area(),
            kExpectedPrintableArea);
}

TEST(PrintSettingsConversionTest, WithValidFlippedImageableArea) {
#if BUILDFLAG(IS_MAC)
  static constexpr gfx::Size kExpectedSize{842, 595};
  static constexpr gfx::Rect kExpectedPrintableArea{0, 85, 839, 510};
#else
  static constexpr gfx::Size kExpectedSize{3508, 2480};
  static constexpr gfx::Rect kExpectedPrintableArea{0, 354, 3496, 2126};
#endif

  base::Value value = base::test::ParseJson(kPrinterSettingsWithImageableArea);
  ASSERT_TRUE(value.is_dict());
  auto& dict = value.GetDict();
  dict.Set("landscape", true);
  std::unique_ptr<PrintSettings> settings = PrintSettingsFromJobSettings(dict);
  ASSERT_TRUE(settings);
  EXPECT_EQ(settings->page_setup_device_units().physical_size(), kExpectedSize);
  EXPECT_EQ(settings->page_setup_device_units().printable_area(),
            kExpectedPrintableArea);
}

TEST(PrintSettingsConversionTest, WithOutOfBoundsImageableArea) {
  base::Value value = base::test::ParseJson(kPrinterSettingsWithImageableArea);
  ASSERT_TRUE(value.is_dict());
  auto& dict = value.GetDict();
  auto* media_size_dict = dict.FindDict("mediaSize");
  ASSERT_TRUE(media_size_dict);
  media_size_dict->Set("imageable_area_left_microns", -500);
  std::unique_ptr<PrintSettings> settings = PrintSettingsFromJobSettings(dict);
  ASSERT_TRUE(settings);
  EXPECT_TRUE(settings->page_setup_device_units().physical_size().IsEmpty());
  EXPECT_TRUE(settings->page_setup_device_units().printable_area().IsEmpty());
}

TEST(PrintSettingsConversionTest, WithMissingImageableAreaValue) {
  base::Value value = base::test::ParseJson(kPrinterSettingsWithImageableArea);
  ASSERT_TRUE(value.is_dict());
  auto& dict = value.GetDict();
  auto* media_size_dict = dict.FindDict("mediaSize");
  ASSERT_TRUE(media_size_dict);
  media_size_dict->Remove("imageable_area_left_microns");
  std::unique_ptr<PrintSettings> settings = PrintSettingsFromJobSettings(dict);
  ASSERT_TRUE(settings);
  EXPECT_TRUE(settings->page_setup_device_units().physical_size().IsEmpty());
  EXPECT_TRUE(settings->page_setup_device_units().printable_area().IsEmpty());
}

TEST(PrintSettingsConversionTest, MissingDeviceName) {
  base::Value value = base::test::ParseJson(kPrinterSettings);
  ASSERT_TRUE(value.is_dict());
  auto& dict = value.GetDict();
  dict.Remove("deviceName");
  EXPECT_FALSE(PrintSettingsFromJobSettings(dict));
}

#if BUILDFLAG(IS_CHROMEOS)
TEST(PrintSettingsConversionTest, DontSendUsername) {
  base::Value value = base::test::ParseJson(kPrinterSettings);
  ASSERT_TRUE(value.is_dict());
  auto& dict = value.GetDict();
  dict.Set(kSettingSendUserInfo, false);
  std::unique_ptr<PrintSettings> settings = PrintSettingsFromJobSettings(dict);
  ASSERT_TRUE(settings);
  EXPECT_FALSE(settings->send_user_info());
  EXPECT_EQ("", settings->username());
}
#endif

#if BUILDFLAG(IS_CHROMEOS) || (BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_CUPS))
TEST(PrintSettingsConversionTest, FilterNonJobSettings) {
  base::Value value = base::test::ParseJson(kPrinterSettings);
  ASSERT_TRUE(value.is_dict());
  auto& dict = value.GetDict();

  {
    base::Value::Dict advanced_attributes;
    advanced_attributes.Set("printer-info", "yada");
    advanced_attributes.Set("printer-make-and-model", "yada");
    advanced_attributes.Set("system_driverinfo", "yada");
    advanced_attributes.Set("Foo", "Bar");
    dict.Set(kSettingAdvancedSettings, std::move(advanced_attributes));
  }

  std::unique_ptr<PrintSettings> settings = PrintSettingsFromJobSettings(dict);
  ASSERT_TRUE(settings);
  EXPECT_EQ(settings->advanced_settings().size(), 1u);
  ASSERT_TRUE(base::Contains(settings->advanced_settings(), "Foo"));
  EXPECT_EQ(settings->advanced_settings().at("Foo"), base::Value("Bar"));
}
#endif  // BUILDFLAG(IS_CHROMEOS) || (BUILDFLAG(IS_LINUX) &&
        // BUILDFLAG(USE_CUPS))

}  // namespace printing
