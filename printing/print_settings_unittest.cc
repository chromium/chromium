// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings.h"

#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
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

}  // namespace printing
