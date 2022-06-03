// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings_conversion.h"

#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
  "pinValue": "0000"
})";

}  // namespace

TEST(PrintSettingsConversionTest, ConversionTest_InvalidSettings) {
  absl::optional<base::Value> value = base::JSONReader::Read("{}");
  ASSERT_TRUE(value.has_value());
  EXPECT_FALSE(PrintSettingsFromJobSettings(value.value()));
}

TEST(PrintSettingsConversionTest, ConversionTest) {
  absl::optional<base::Value> value = base::JSONReader::Read(kPrinterSettings);
  ASSERT_TRUE(value.has_value());
  std::unique_ptr<PrintSettings> settings =
      PrintSettingsFromJobSettings(value.value());
  ASSERT_TRUE(settings);
#if defined(OS_CHROMEOS)
  EXPECT_TRUE(settings->send_user_info());
  EXPECT_EQ("username@domain.net", settings->username());
  EXPECT_EQ("0000", settings->pin_value());
#endif
  EXPECT_EQ(settings->dpi_horizontal(), 300);
  EXPECT_EQ(settings->dpi_vertical(), 300);
  value->SetIntKey("dpiVertical", 600);
  settings = PrintSettingsFromJobSettings(value.value());
  ASSERT_TRUE(settings);
  EXPECT_EQ(settings->rasterize_pdf_dpi(), 150);
  EXPECT_EQ(settings->dpi_horizontal(), 300);
  EXPECT_EQ(settings->dpi_vertical(), 600);
  EXPECT_TRUE(value->RemoveKey("dpiVertical"));
  settings = PrintSettingsFromJobSettings(value.value());
  EXPECT_FALSE(settings);
}

#if defined(OS_CHROMEOS)
TEST(PrintSettingsConversionTest, ConversionTest_DontSendUsername) {
  absl::optional<base::Value> value = base::JSONReader::Read(kPrinterSettings);
  ASSERT_TRUE(value.has_value());
  value->SetKey(kSettingSendUserInfo, base::Value(false));
  std::unique_ptr<PrintSettings> settings =
      PrintSettingsFromJobSettings(value.value());
  ASSERT_TRUE(settings);
  EXPECT_FALSE(settings->send_user_info());
  EXPECT_EQ("", settings->username());
}
#endif

#if defined(OS_CHROMEOS) || (defined(OS_LINUX) && defined(USE_CUPS))
TEST(PrintSettingsConversionTest, FilterNonJobSettings) {
  absl::optional<base::Value> value = base::JSONReader::Read(kPrinterSettings);
  ASSERT_TRUE(value.has_value());

  {
    base::Value advanced_attributes(base::Value::Type::DICTIONARY);
    advanced_attributes.SetStringKey("printer-info", "yada");
    advanced_attributes.SetStringKey("printer-make-and-model", "yada");
    advanced_attributes.SetStringKey("system_driverinfo", "yada");
    advanced_attributes.SetStringKey("Foo", "Bar");
    value->SetKey(kSettingAdvancedSettings, std::move(advanced_attributes));
  }

  std::unique_ptr<PrintSettings> settings =
      PrintSettingsFromJobSettings(value.value());
  ASSERT_TRUE(settings);
  EXPECT_EQ(settings->advanced_settings().size(), 1u);
  ASSERT_TRUE(base::Contains(settings->advanced_settings(), "Foo"));
  EXPECT_EQ(settings->advanced_settings().at("Foo"), base::Value("Bar"));
}
#endif  // defined(OS_CHROMEOS) || (defined(OS_LINUX) && defined(USE_CUPS))

}  // namespace printing
