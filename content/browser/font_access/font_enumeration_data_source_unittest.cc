// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/i18n/rtl.h"
#include "content/browser/font_access/font_enumeration_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"

#if BUILDFLAG(IS_APPLE)
#include "content/browser/font_access/font_enumeration_data_source_mac.h"
#endif  // BUILDFLAG(IS_APPLE)

namespace content {

namespace {

class FontEnumerationDataSourceTest : public testing::Test {
 public:
  FontEnumerationDataSourceTest() {
    data_source_ = FontEnumerationDataSource::Create();
  }

 protected:
  std::unique_ptr<FontEnumerationDataSource> data_source_;
};

TEST_F(FontEnumerationDataSourceTest, GetFonts_EnUsLocale) {
  blink::FontEnumerationTable font_table = data_source_->GetFonts("en-us");

  if (FontEnumerationDataSource::IsOsSupported()) {
    EXPECT_GT(font_table.fonts_size(), 0);
  } else {
    EXPECT_EQ(font_table.fonts_size(), 0);
  }
}

TEST_F(FontEnumerationDataSourceTest, GetFonts_DefaultLocale) {
  std::string locale = base::i18n::GetConfiguredLocale();
  blink::FontEnumerationTable font_table = data_source_->GetFonts(locale);

  if (FontEnumerationDataSource::IsOsSupported()) {
    EXPECT_GT(font_table.fonts_size(), 0);
  } else {
    EXPECT_EQ(font_table.fonts_size(), 0);
  }
}

#if BUILDFLAG(IS_APPLE)
TEST_F(FontEnumerationDataSourceTest, CheckInvalidFontMac) {
  const CTFontDescriptorRef fd = nullptr;
  FontEnumerationDataSourceMac data_src_mac;
  EXPECT_FALSE(data_src_mac.IsValidFontMacForTesting(fd));
}
#endif  // BUILDFLAG(IS_APPLE)

}  // namespace

}  // namespace content
