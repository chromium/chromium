// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ui/document_properties.h"

#include <optional>
#include <string>

#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "pdf/document_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace chrome_pdf {

namespace {

using ::testing::IsEmpty;

class FormatPageSizeTest : public testing::Test {
 protected:
  void SetUp() override {
    const std::string locale(GetLocale());
    if (!locale.empty()) {
      base::i18n::SetICUDefaultLocale(locale);
      base::ResetFormattersForTesting();
    }
  }

  void TearDown() override {
    base::i18n::SetICUDefaultLocale(default_locale_);
    base::ResetFormattersForTesting();
  }

  virtual std::string GetLocale() const { return std::string(); }

 private:
  std::string default_locale_{base::i18n::GetConfiguredLocale()};
};

}  // namespace

TEST_F(FormatPageSizeTest, NoUniformSize) {
  EXPECT_EQ(FormatPageSize(std::nullopt), u"Varies");
}

class FormatPageSizeMillimetersTest : public FormatPageSizeTest {
 protected:
  std::string GetLocale() const override { return "en_CA"; }
};

TEST_F(FormatPageSizeMillimetersTest, EmptySize) {
  EXPECT_EQ(FormatPageSize(gfx::Size()), u"0 × 0 mm (square)");
}

TEST_F(FormatPageSizeMillimetersTest, Portrait) {
  EXPECT_EQ(FormatPageSize(gfx::Size(100, 200)), u"35 × 71 mm (portrait)");
}

TEST_F(FormatPageSizeMillimetersTest, Landscape) {
  EXPECT_EQ(FormatPageSize(gfx::Size(200, 100)), u"71 × 35 mm (landscape)");
}

TEST_F(FormatPageSizeMillimetersTest, Square) {
  EXPECT_EQ(FormatPageSize(gfx::Size(100, 100)), u"35 × 35 mm (square)");
}

TEST_F(FormatPageSizeMillimetersTest, Large) {
  EXPECT_EQ(FormatPageSize(gfx::Size(72000, 72000)),
            u"25,400 × 25,400 mm (square)");
}

class FormatPageSizeMillimetersPeriodSeparatorTest : public FormatPageSizeTest {
 protected:
  std::string GetLocale() const override { return "de_DE"; }
};

TEST_F(FormatPageSizeMillimetersPeriodSeparatorTest, Large) {
  EXPECT_EQ(FormatPageSize(gfx::Size(72000, 72000)),
            u"25.400 × 25.400 mm (square)");
}

class FormatPageSizeInchesTest : public FormatPageSizeTest {
 protected:
  std::string GetLocale() const override { return "en_US"; }
};

TEST_F(FormatPageSizeInchesTest, EmptySize) {
  EXPECT_EQ(FormatPageSize(gfx::Size()), u"0.00 × 0.00 in (square)");
}

TEST_F(FormatPageSizeInchesTest, Portrait) {
  EXPECT_EQ(FormatPageSize(gfx::Size(100, 200)), u"1.39 × 2.78 in (portrait)");
}

TEST_F(FormatPageSizeInchesTest, Landscape) {
  EXPECT_EQ(FormatPageSize(gfx::Size(200, 100)), u"2.78 × 1.39 in (landscape)");
}

TEST_F(FormatPageSizeInchesTest, Square) {
  EXPECT_EQ(FormatPageSize(gfx::Size(100, 100)), u"1.39 × 1.39 in (square)");
}

TEST_F(FormatPageSizeInchesTest, Large) {
  EXPECT_EQ(FormatPageSize(gfx::Size(72000, 72000)),
            u"1,000.00 × 1,000.00 in (square)");
}

TEST(FormatPdfVersion, Valid) {
  EXPECT_EQ(FormatPdfVersion(PdfVersion::k1_7), "1.7");
  EXPECT_EQ(FormatPdfVersion(PdfVersion::k2_0), "2.0");
}

TEST(FormatPdfVersion, Invalid) {
  EXPECT_THAT(FormatPdfVersion(PdfVersion::kUnknown), IsEmpty());
  EXPECT_THAT(FormatPdfVersion(PdfVersion::k1_8), IsEmpty());
}

}  // namespace chrome_pdf
