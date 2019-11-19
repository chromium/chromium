// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings_conversion.h"

#include "base/json/json_reader.h"
#include "base/values.h"
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
  "pagesPerSheet": 1,
  "dpiHorizontal": 300,
  "dpiVertical": 300,
  "previewModifiable": true,
  "sendUserInfo": true,
  "username": "username@domain.net",
  "pinValue": "0000"
})";

}  // namespace

TEST(PrintSettingsConversionTest, ConversionTest) {
  base::Optional<base::Value> value = base::JSONReader::Read(kPrinterSettings);
  ASSERT_TRUE(value.has_value());
  PrintSettings settings;
  bool success = PrintSettingsFromJobSettings(value.value(), &settings);
  ASSERT_TRUE(success);
#if defined(OS_CHROMEOS)
  EXPECT_TRUE(settings.send_user_info());
  EXPECT_EQ("username@domain.net", settings.username());
  EXPECT_EQ("0000", settings.pin_value());
#endif
}

#if defined(OS_CHROMEOS)
TEST(PrintSettingsConversionTest, ConversionTest_DontSendUsername) {
  base::Optional<base::Value> value = base::JSONReader::Read(kPrinterSettings);
  ASSERT_TRUE(value.has_value());
  value->SetKey(kSettingSendUserInfo, base::Value(false));
  PrintSettings settings;
  bool success = PrintSettingsFromJobSettings(value.value(), &settings);
  ASSERT_TRUE(success);
  EXPECT_FALSE(settings.send_user_info());
  EXPECT_EQ("", settings.username());
}
#endif

}  // namespace printing
