// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/font_unique_name_lookup/icu_fold_case_util.h"

namespace {
const char kTestFilePath1[] = "tmp/test/font1.ttf";
const char kDummyAndroidBuildFingerPrint[] = "A";

void PopulateFontUniqueNameEntry(
    blink::FontUniqueNameTable* font_unique_name_table,
    const std::string& path,
    int32_t ttc_index,
    const std::set<std::string>& names) {
  auto* font_entry = font_unique_name_table->add_fonts();
  font_entry->set_file_path(path);
  font_entry->set_ttc_index(ttc_index);

  std::set<std::string> names_folded;
  for (auto& name : names) {
    names_folded.insert(blink::IcuFoldCase(name));
  }

  // Set iteration will return values in sorted order.
  for (auto& name : names_folded) {
    auto* names_entry = font_unique_name_table->add_name_map();
    names_entry->set_font_name(name);
    names_entry->set_font_index(0);
  }
}

}  // namespace

namespace blink {

class FontTableMatcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    FontUniqueNameTable font_unique_name_table;
    font_unique_name_table.set_stored_for_platform_version_identifier(
        kDummyAndroidBuildFingerPrint);
    PopulateFontUniqueNameEntry(
        &font_unique_name_table, kTestFilePath1, 0,
        {"FONT NAME UPPERCASE", "எழுத்துரு பெயர்", "字體名稱",
         "FONT-NAME-UPPERCASE", "எழுத்துரு-பெயர்", "字體名稱"});
    base::ReadOnlySharedMemoryMapping mapping =
        FontTableMatcher::MemoryMappingFromFontUniqueNameTable(
            std::move(font_unique_name_table));

    matcher_ = std::make_unique<FontTableMatcher>(mapping);
  }

  std::unique_ptr<FontTableMatcher> matcher_;
};

TEST_F(FontTableMatcherTest, CaseInsensitiveMatchingBothNames) {
  ASSERT_EQ(matcher_->AvailableFonts(), 1u);
  std::optional<FontTableMatcher::MatchResult> result =
      matcher_->MatchName("font name uppercase");
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->font_path, kTestFilePath1);
  ASSERT_EQ(result->ttc_index, 0u);

  result = matcher_->MatchName("font-name-uppercase");
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->font_path, kTestFilePath1);
  ASSERT_EQ(result->ttc_index, 0u);
}

TEST_F(FontTableMatcherTest, MatchTamilChinese) {
  ASSERT_EQ(matcher_->AvailableFonts(), 1u);
  for (std::string font_name : {"எழுத்துரு பெயர்", "எழுத்துரு-பெயர்", "字體名稱"}) {
    std::optional<FontTableMatcher::MatchResult> result =
        matcher_->MatchName(font_name);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->font_path, kTestFilePath1);
    ASSERT_EQ(result->ttc_index, 0u);

    std::optional<FontTableMatcher::MatchResult> result_for_substring =
        matcher_->MatchName(font_name.substr(0, font_name.size() - 2u));
    ASSERT_FALSE(result_for_substring.has_value());
  }
}

TEST_F(FontTableMatcherTest, NoSubStringMatching) {
  ASSERT_EQ(matcher_->AvailableFonts(), 1u);
  std::optional<FontTableMatcher::MatchResult> result =
      matcher_->MatchName("font name");
  ASSERT_FALSE(result.has_value());

  result = matcher_->MatchName("font-name");
  ASSERT_FALSE(result.has_value());
}

}  // namespace blink
