// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings_conversion_chromeos.h"

#include <optional>
#include <string>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "printing/mojom/print.mojom.h"

namespace printing {

namespace {

const base::Value::List kClientInfoJobSetting = base::test::ParseJsonList(R"([
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
  ])");

const std::vector<mojom::IppClientInfo> kClientInfo{
    mojom::IppClientInfo(mojom::IppClientInfo::ClientType::kOperatingSystem,
                         "ChromeOS",
                         "patch",
                         "str_version",
                         "version"),
    mojom::IppClientInfo(mojom::IppClientInfo::ClientType::kOther,
                         "chromebook-{DEVICE_ASSET_ID}",
                         std::nullopt,
                         "",
                         std::nullopt)};

TEST(PrintSettingsConversionChromeosTest, ConvertClientInfoToJobSetting) {
  base::Value::List job_setting = ConvertClientInfoToJobSetting(kClientInfo);
  EXPECT_EQ(job_setting, kClientInfoJobSetting);
}

TEST(PrintSettingsConversionChromeosTest, ConvertJobSettingToClientInfo) {
  std::vector<mojom::IppClientInfo> client_info =
      ConvertJobSettingToClientInfo(kClientInfoJobSetting);

  EXPECT_EQ(client_info, kClientInfo);
}

}  // namespace

}  // namespace printing
