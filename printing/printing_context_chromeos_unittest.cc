// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_chromeos.h"

#include <string.h>

#include "printing/backend/cups_ipp_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

const char* GetOptionValue(const std::vector<ScopedCupsOption>& options,
                           const char* option_name) {
  DCHECK(option_name);
  const char* ret = nullptr;
  for (const auto& option : options) {
    EXPECT_TRUE(option->name);
    EXPECT_TRUE(option->value);
    if (option->name && !strcmp(option_name, option->name)) {
      EXPECT_EQ(nullptr, ret)
          << "Multiple options with name " << option_name << " found.";
      ret = option->value;
    }
  }
  return ret;
}

class TestPrintSettings : public PrintSettings {
 public:
  TestPrintSettings() { set_duplex_mode(mojom::DuplexMode::kSimplex); }
};

class PrintingContextTest : public testing::Test {
 public:
  const char* Get(const char* name) const {
    return GetOptionValue(SettingsToCupsOptions(settings_), name);
  }
  TestPrintSettings settings_;
};

TEST_F(PrintingContextTest, SettingsToCupsOptions_Color) {
  settings_.set_color(mojom::ColorModel::kGray);
  EXPECT_STREQ("monochrome", Get(kIppColor));
  settings_.set_color(mojom::ColorModel::kColor);
  EXPECT_STREQ("color", Get(kIppColor));
}

TEST_F(PrintingContextTest, SettingsToCupsOptions_Duplex) {
  settings_.set_duplex_mode(mojom::DuplexMode::kSimplex);
  EXPECT_STREQ("one-sided", Get(kIppDuplex));
  settings_.set_duplex_mode(mojom::DuplexMode::kLongEdge);
  EXPECT_STREQ("two-sided-long-edge", Get(kIppDuplex));
  settings_.set_duplex_mode(mojom::DuplexMode::kShortEdge);
  EXPECT_STREQ("two-sided-short-edge", Get(kIppDuplex));
}

TEST_F(PrintingContextTest, SettingsToCupsOptions_Media) {
  EXPECT_STREQ("", Get(kIppMedia));
  settings_.set_requested_media(
      {gfx::Size(297000, 420000), "iso_a3_297x420mm"});
  EXPECT_STREQ("iso_a3_297x420mm", Get(kIppMedia));
}

TEST_F(PrintingContextTest, SettingsToCupsOptions_Copies) {
  settings_.set_copies(3);
  EXPECT_STREQ("3", Get(kIppCopies));
}

TEST_F(PrintingContextTest, SettingsToCupsOptions_Collate) {
  EXPECT_STREQ("separate-documents-uncollated-copies", Get(kIppCollate));
  settings_.set_collate(true);
  EXPECT_STREQ("separate-documents-collated-copies", Get(kIppCollate));
}

TEST_F(PrintingContextTest, SettingsToCupsOptions_Pin) {
  EXPECT_STREQ(nullptr, Get(kIppPin));
  settings_.set_pin_value("1234");
  EXPECT_STREQ("1234", Get(kIppPin));
}

TEST_F(PrintingContextTest, SettingsToCupsOptions_Resolution) {
  EXPECT_STREQ(nullptr, Get(kIppResolution));
  settings_.set_dpi_xy(0, 300);
  EXPECT_STREQ(nullptr, Get(kIppResolution));
  settings_.set_dpi_xy(300, 0);
  EXPECT_STREQ(nullptr, Get(kIppResolution));
  settings_.set_dpi(600);
  EXPECT_STREQ("600dpi", Get(kIppResolution));
  settings_.set_dpi_xy(600, 1200);
  EXPECT_STREQ("600x1200dpi", Get(kIppResolution));
}

}  // namespace

}  // namespace printing
