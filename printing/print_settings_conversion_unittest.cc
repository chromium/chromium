// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings_conversion.h"

#include "base/containers/contains.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
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
  "pinValue": "0000",
  "ipp-client-info": [
    {
      "ipp-client-name": "ChromeOS",
      "ipp-client-patches": "patch",
      "ipp-client-string-version": "str_version",
      "ipp-client-type": 4,
      "ipp-client-version": "version",
    },
    {
      "ipp-client-name": "chromebook-{DEVICE_ASSET_ID}",
      "ipp-client-string-version": "",
      "ipp-client-type": 6,
    }
  ],
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

#if !BUILDFLAG(IS_MAC)
const char kPrinterSettingsWithNonSquarePixels[] = R"({
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
  "dpiHorizontal": 800,
  "dpiVertical": 50,
})";
#endif  // !BUILDFLAG(IS_MAC)

const char kCustomMargins[] = R"({
  "marginBottom": 10,
  "marginLeft": 30,
  "marginRight": 20,
  "marginTop": 80
})";

}  // namespace

TEST(PrintSettingsConversionTest, InvalidSettings) {
  base::Value::Dict dict = base::test::ParseJsonDict("{}");
  ASSERT_TRUE(dict.empty());
  EXPECT_FALSE(PrintSettingsFromJobSettings(dict));
}

TEST(PrintSettingsConversionTest, Conversion) {
  base::Value::Dict dict = base::test::ParseJsonDict(kPrinterSettings);
  std::unique_ptr<PrintSettings> settings = PrintSettingsFromJobSettings(dict);
  ASSERT_TRUE(settings);
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(settings->send_user_info());
  EXPECT_EQ("username@domain.net", settings->username());
  EXPECT_EQ("this is an OAuth access token", settings->oauth_token());
  EXPECT_EQ("0000", settings->pin_value());

  ASSERT_EQ(settings->client_infos().size(), 2u);
  EXPECT_EQ(settings->client_infos()[0].client_name, "ChromeOS");
  EXPECT_EQ(settings->client_infos()[0].client_type,
            mojom::IppClientInfo::ClientType::kOperatingSystem);
  EXPECT_EQ(settings->client_infos()[0].client_patches, "patch");
  EXPECT_EQ(settings->client_infos()[0].client_string_version, "str_version");
  EXPECT_EQ(settings->client_infos()[0].client_version, "version");
  EXPECT_EQ(settings->client_infos()[1].client_name,
            "chromebook-{DEVICE_ASSET_ID}");
  EXPECT_EQ(settings->client_infos()[1].client_type,
            mojom::IppClientInfo::ClientType::kOther);
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
  const PageMargins kExpectedPageMargins(0, 3, 28, 85, 28, 28);
#else
  static constexpr gfx::Size kExpectedSize{2480, 3508};
  static constexpr gfx::Rect kExpectedPrintableArea{0, 0, 2126, 3496};
  const PageMargins kExpectedPageMargins(0, 12, 118, 354, 118, 118);
#endif

  base::Value::Dict dict =
      base::test::ParseJsonDict(kPrinterSettingsWithImageableArea);
  std::unique_ptr<PrintSettings> settings = PrintSettingsFromJobSettings(dict);
  ASSERT_TRUE(settings);
  EXPECT_EQ(settings->dpi_horizontal(), 300);
  EXPECT_EQ(settings->dpi_vertical(), 300);
  EXPECT_EQ(settings->page_setup_device_units().physical_size(), kExpectedSize);
  EXPECT_EQ(settings->page_setup_device_units().printable_area(),
            kExpectedPrintableArea);
  EXPECT_EQ(settings->page_setup_device_units().effective_margins(),
            kExpectedPageMargins);
}

TEST(PrintSettingsConversionTest, WithValidFlippedImageableArea) {
#if BUILDFLAG(IS_MAC)
  static constexpr gfx::Size kExpectedSize{842, 595};
  static constexpr gfx::Rect kExpectedPrintableArea{0, 85, 839, 510};
  const PageMargins kExpectedPageMargins(85, 0, 28, 28, 85, 28);
#else
  static constexpr gfx::Size kExpectedSize{3508, 2480};
  static constexpr gfx::Rect kExpectedPrintableArea{0, 354, 3496, 2126};
  const PageMargins kExpectedPageMargins(354, 0, 118, 118, 354, 118);
#endif

  base::Value::Dict dict =
      base::test::ParseJsonDict(kPrinterSettingsWithImageableArea);
  dict.Set("landscape", true);
  std::unique_ptr<PrintSettings> settings = PrintSettingsFromJobSettings(dict);
  ASSERT_TRUE(settings);
  EXPECT_EQ(settings->page_setup_device_units().physical_size(), kExpectedSize);
  EXPECT_EQ(settings->page_setup_device_units().printable_area(),
            kExpectedPrintableArea);
  EXPECT_EQ(settings->page_setup_device_units().effective_margins(),
            kExpectedPageMargins);
}

TEST(PrintSettingsConversionTest, WithOutOfBoundsImageableArea) {
  base::Value::Dict dict =
      base::test::ParseJsonDict(kPrinterSettingsWithImageableArea);
  auto* media_size_dict = dict.FindDict("mediaSize");
  ASSERT_TRUE(media_size_dict);
  media_size_dict->Set("imageable_area_left_microns", -500);
  std::unique_ptr<PrintSettings> settings = PrintSettingsFromJobSettings(dict);
  ASSERT_TRUE(settings);
  EXPECT_TRUE(settings->page_setup_device_units().physical_size().IsEmpty());
  EXPECT_TRUE(settings->page_setup_device_units().printable_area().IsEmpty());
  EXPECT_EQ(settings->page_setup_device_units().effective_margins(),
            PageMargins(0, 0, 0, 0, 0, 0));
}

TEST(PrintSettingsConversionTest, WithMissingImageableAreaValue) {
  base::Value::Dict dict =
      base::test::ParseJsonDict(kPrinterSettingsWithImageableArea);
  auto* media_size_dict = dict.FindDict("mediaSize");
  ASSERT_TRUE(media_size_dict);
  media_size_dict->Remove("imageable_area_left_microns");
  std::unique_ptr<PrintSettings> settings = PrintSettingsFromJobSettings(dict);
  ASSERT_TRUE(settings);
  EXPECT_TRUE(settings->page_setup_device_units().physical_size().IsEmpty());
  EXPECT_TRUE(settings->page_setup_device_units().printable_area().IsEmpty());
  EXPECT_EQ(settings->page_setup_device_units().effective_margins(),
            PageMargins(0, 0, 0, 0, 0, 0));
}

TEST(PrintSettingsConversionTest, WithCustomMarginsAndImageableArea) {
  // Test imageable area with custom margins.
#if BUILDFLAG(IS_MAC)
  static constexpr gfx::Size kExpectedSize{595, 842};
  static constexpr gfx::Rect kExpectedPrintableArea{0, 0, 510, 839};
  const PageMargins kExpectedPageMargins(0, 0, 30, 20, 80, 10);
#else
  static constexpr gfx::Size kExpectedSize{2480, 3508};
  static constexpr gfx::Rect kExpectedPrintableArea{0, 0, 2126, 3496};
  const PageMargins kExpectedPageMargins(0, 0, 125, 83, 333, 42);
#endif

  base::Value::Dict dict =
      base::test::ParseJsonDict(kPrinterSettingsWithImageableArea);
  dict.Set("marginsType", static_cast<int>(mojom::MarginType::kCustomMargins));
  dict.Set("marginsCustom", base::test::ParseJson(kCustomMargins));
  std::unique_ptr<PrintSettings> settings = PrintSettingsFromJobSettings(dict);
  ASSERT_TRUE(settings);
  const PageSetup& page_setup = settings->page_setup_device_units();
  EXPECT_EQ(page_setup.physical_size(), kExpectedSize);
  EXPECT_EQ(page_setup.printable_area(), kExpectedPrintableArea);
  EXPECT_EQ(page_setup.effective_margins(), kExpectedPageMargins);
}

#if !BUILDFLAG(IS_MAC)
TEST(PrintSettingsConversionTest, WithNonSquarePixels) {
  // Check that physical size and printable area are scaled by the max DPI
  // value. Not needed for macOS, which always has square pixels.
  static constexpr gfx::Size kExpectedSize{6614, 9354};
  static constexpr gfx::Rect kExpectedPrintableArea{0, 0, 5669, 9323};

  base::Value::Dict dict =
      base::test::ParseJsonDict(kPrinterSettingsWithNonSquarePixels);
  std::unique_ptr<PrintSettings> settings = PrintSettingsFromJobSettings(dict);
  ASSERT_TRUE(settings);
  EXPECT_EQ(settings->dpi_horizontal(), 800);
  EXPECT_EQ(settings->dpi_vertical(), 50);
  EXPECT_EQ(settings->page_setup_device_units().physical_size(), kExpectedSize);
  EXPECT_EQ(settings->page_setup_device_units().printable_area(),
            kExpectedPrintableArea);
}
#endif  // !BUILDFLAG(IS_MAC)

TEST(PrintSettingsConversionTest, MissingDeviceName) {
  base::Value::Dict dict = base::test::ParseJsonDict(kPrinterSettings);
  EXPECT_TRUE(dict.Remove("deviceName"));
  EXPECT_FALSE(PrintSettingsFromJobSettings(dict));
}

#if BUILDFLAG(IS_CHROMEOS)
TEST(PrintSettingsConversionTest, DontSendUsername) {
  base::Value::Dict dict = base::test::ParseJsonDict(kPrinterSettings);
  dict.Set(kSettingSendUserInfo, false);
  std::unique_ptr<PrintSettings> settings = PrintSettingsFromJobSettings(dict);
  ASSERT_TRUE(settings);
  EXPECT_FALSE(settings->send_user_info());
  EXPECT_EQ("", settings->username());
}
#endif

#if BUILDFLAG(IS_CHROMEOS) || (BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_CUPS))
TEST(PrintSettingsConversionTest, FilterNonJobSettings) {
  base::Value::Dict dict = base::test::ParseJsonDict(kPrinterSettings);

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
