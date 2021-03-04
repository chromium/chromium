// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ui/format_page_size.h"

#include <string>

#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/mock_resource_bundle_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"

using ::testing::Invoke;
using ::testing::NiceMock;

namespace chrome_pdf {

namespace {

bool GetPageSizeString(int message_id, base::string16* value) {
  std::string utf8;
  switch (message_id) {
    case IDS_PDF_PROPERTIES_PAGE_SIZE_VALUE_INCH:
      utf8 = "$1 × $2 in ($3)";
      break;
    case IDS_PDF_PROPERTIES_PAGE_SIZE_VALUE_MM:
      utf8 = "$1 × $2 mm ($3)";
      break;
    case IDS_PDF_PROPERTIES_PAGE_SIZE_PORTRAIT:
      utf8 = "portrait";
      break;
    case IDS_PDF_PROPERTIES_PAGE_SIZE_LANDSCAPE:
      utf8 = "landscape";
      break;
    case IDS_PDF_PROPERTIES_PAGE_SIZE_VARIABLE:
      utf8 = "Varies";
      break;
    default:
      return false;
  }

  *value = base::UTF8ToUTF16(utf8);
  return true;
}

class FormatPageSizeTest : public testing::Test {
 protected:
  void SetUp() override {
    // `pdf_unittests` does not have a resource bundle that needs to be restored
    // at the end of the test.
    ASSERT_FALSE(ui::ResourceBundle::HasSharedInstance());

    ON_CALL(mock_resource_delegate_, GetLocalizedString)
        .WillByDefault(Invoke(GetPageSizeString));

    const std::string locale(GetLocale());
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        locale, &mock_resource_delegate_,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);

    if (!locale.empty()) {
      base::i18n::SetICUDefaultLocale(locale);
      base::ResetFormattersForTesting();
    }
  }

  void TearDown() override {
    ASSERT_TRUE(ui::ResourceBundle::HasSharedInstance());
    ui::ResourceBundle::CleanupSharedInstance();
    base::i18n::SetICUDefaultLocale(default_locale_);
    base::ResetFormattersForTesting();
  }

  virtual std::string GetLocale() const { return std::string(); }

 private:
  std::string default_locale_{base::i18n::GetConfiguredLocale()};
  NiceMock<ui::MockResourceBundleDelegate> mock_resource_delegate_;
};

std::string FormatPageSizeUtf8(const base::Optional<gfx::Size>& size_points) {
  return base::UTF16ToUTF8(FormatPageSize(size_points));
}

}  // namespace

TEST_F(FormatPageSizeTest, NoUniformSize) {
  EXPECT_EQ(FormatPageSizeUtf8(base::nullopt), "Varies");
}

class FormatPageSizeMillimetersTest : public FormatPageSizeTest {
 protected:
  std::string GetLocale() const override { return "en_CA"; }
};

TEST_F(FormatPageSizeMillimetersTest, EmptySize) {
  EXPECT_EQ(FormatPageSizeUtf8(gfx::Size()), "0 × 0 mm (portrait)");
}

TEST_F(FormatPageSizeMillimetersTest, Portrait) {
  EXPECT_EQ(FormatPageSizeUtf8(gfx::Size(100, 200)), "35 × 71 mm (portrait)");
}

TEST_F(FormatPageSizeMillimetersTest, Landscape) {
  EXPECT_EQ(FormatPageSizeUtf8(gfx::Size(200, 100)), "71 × 35 mm (landscape)");
}

TEST_F(FormatPageSizeMillimetersTest, Square) {
  EXPECT_EQ(FormatPageSizeUtf8(gfx::Size(100, 100)), "35 × 35 mm (portrait)");
}

TEST_F(FormatPageSizeMillimetersTest, Large) {
  EXPECT_EQ(FormatPageSizeUtf8(gfx::Size(72000, 72000)),
            "25,400 × 25,400 mm (portrait)");
}

class FormatPageSizeMillimetersPeriodSeparatorTest : public FormatPageSizeTest {
 protected:
  std::string GetLocale() const override { return "de_DE"; }
};

TEST_F(FormatPageSizeMillimetersPeriodSeparatorTest, Large) {
  EXPECT_EQ(FormatPageSizeUtf8(gfx::Size(72000, 72000)),
            "25.400 × 25.400 mm (portrait)");
}

class FormatPageSizeInchesTest : public FormatPageSizeTest {
 protected:
  std::string GetLocale() const override { return "en_US"; }
};

TEST_F(FormatPageSizeInchesTest, EmptySize) {
  EXPECT_EQ(FormatPageSizeUtf8(gfx::Size()), "0.00 × 0.00 in (portrait)");
}

TEST_F(FormatPageSizeInchesTest, Portrait) {
  EXPECT_EQ(FormatPageSizeUtf8(gfx::Size(100, 200)),
            "1.39 × 2.78 in (portrait)");
}

TEST_F(FormatPageSizeInchesTest, Landscape) {
  EXPECT_EQ(FormatPageSizeUtf8(gfx::Size(200, 100)),
            "2.78 × 1.39 in (landscape)");
}

TEST_F(FormatPageSizeInchesTest, Square) {
  EXPECT_EQ(FormatPageSizeUtf8(gfx::Size(100, 100)),
            "1.39 × 1.39 in (portrait)");
}

TEST_F(FormatPageSizeInchesTest, Large) {
  EXPECT_EQ(FormatPageSizeUtf8(gfx::Size(72000, 72000)),
            "1,000.00 × 1,000.00 in (portrait)");
}

}  // namespace chrome_pdf
