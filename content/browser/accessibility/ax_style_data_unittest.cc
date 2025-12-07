// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/ax_style_data.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom-data-view.h"

namespace content {

using testing::Pair;
using testing::UnorderedElementsAre;

using RangePairs = AXStyleData::RangePairs;

class AXStyleDataTest : public testing::Test {
 protected:
  AXStyleData style_data_;
};

TEST_F(AXStyleDataTest, AddRange_AllFields) {
  AXStyleData::AddRange(style_data_.suggestions, std::u16string(), 5, 10);
  EXPECT_THAT(
      *style_data_.suggestions,
      UnorderedElementsAre(Pair(std::u16string(), RangePairs{{5, 10}})));

  AXStyleData::AddRange(style_data_.links, std::u16string(u"example.com"), 8,
                        12);
  AXStyleData::AddRange(style_data_.links, std::u16string(u"other.com"), 3, 7);
  EXPECT_THAT(*style_data_.links,
              UnorderedElementsAre(
                  Pair(std::u16string(u"example.com"), RangePairs{{8, 12}}),
                  Pair(std::u16string(u"other.com"), RangePairs{{3, 7}})));

  AXStyleData::AddRange(style_data_.text_sizes, 24.0f, 15, 20);
  AXStyleData::AddRange(style_data_.text_sizes, 24.0f, 18, 23);
  EXPECT_THAT(
      *style_data_.text_sizes,
      UnorderedElementsAre(Pair(24.0f, RangePairs{{15, 20}, {18, 23}})));

  AXStyleData::AddRange(style_data_.text_styles, ax::mojom::TextStyle::kBold, 5,
                        10);
  AXStyleData::AddRange(style_data_.text_styles, ax::mojom::TextStyle::kItalic,
                        10, 20);
  EXPECT_THAT(*style_data_.text_styles,
              UnorderedElementsAre(
                  Pair(ax::mojom::TextStyle::kBold, RangePairs{{5, 10}}),
                  Pair(ax::mojom::TextStyle::kItalic, RangePairs{{10, 20}})));

  AXStyleData::AddRange(style_data_.text_positions,
                        ax::mojom::TextPosition::kSuperscript, 5, 10);
  EXPECT_THAT(*style_data_.text_positions,
              UnorderedElementsAre(Pair(ax::mojom::TextPosition::kSuperscript,
                                        RangePairs{{5, 10}})));

  AXStyleData::AddRange<int>(style_data_.foreground_colors, 0xFF0000FF, 5, 10);
  AXStyleData::AddRange<int>(style_data_.foreground_colors, 0xFF000000, 10, 20);
  AXStyleData::AddRange<int>(style_data_.foreground_colors, 0xFF0000FF, 20, 30);
  EXPECT_THAT(
      *style_data_.foreground_colors,
      UnorderedElementsAre(
          Pair(static_cast<int>(0xFF0000FF), RangePairs{{5, 10}, {20, 30}}),
          Pair(static_cast<int>(0xFF000000), RangePairs{{10, 20}})));

  AXStyleData::AddRange<int>(style_data_.background_colors, 0xFFFF0000, 5, 20);
  AXStyleData::AddRange<int>(style_data_.background_colors, 0xFFFF0000, 20, 30);
  AXStyleData::AddRange<int>(style_data_.background_colors, 0xFFFF0000, 10, 10);
  EXPECT_THAT(
      *style_data_.background_colors,
      UnorderedElementsAre(Pair(static_cast<int>(0xFFFF0000),
                                RangePairs{{5, 20}, {20, 30}, {10, 10}})));

  AXStyleData::AddRange(style_data_.font_families, std::string("Serif"), 5, 10);
  EXPECT_THAT(*style_data_.font_families,
              UnorderedElementsAre(Pair("Serif", RangePairs{{5, 10}})));

  AXStyleData::AddRange(style_data_.locales, std::string("zh-TW"), 5, 10);
  EXPECT_THAT(*style_data_.locales,
              UnorderedElementsAre(Pair("zh-TW", RangePairs{{5, 10}})));
}

TEST_F(AXStyleDataTest, AddRange) {
  AXStyleData::AddRange(style_data_.text_sizes, 24.0f, 10, 20);
  AXStyleData::AddRange(style_data_.text_sizes, 24.0f, 30, 40);
  EXPECT_THAT(
      *style_data_.text_sizes,
      UnorderedElementsAre(Pair(24.0f, RangePairs{{10, 20}, {30, 40}})));
}

TEST_F(AXStyleDataTest, AddRange_EmptyRange) {
  AXStyleData::AddRange(style_data_.text_sizes, 24.0f, 10, 10);
  EXPECT_THAT(*style_data_.text_sizes,
              UnorderedElementsAre(Pair(24.0f, RangePairs{{10, 10}})));
}

TEST_F(AXStyleDataTest, AddRange_SameRange) {
  AXStyleData::AddRange(style_data_.text_sizes, 24.0f, 10, 20);
  AXStyleData::AddRange(style_data_.text_sizes, 24.0f, 10, 20);
  EXPECT_THAT(
      *style_data_.text_sizes,
      UnorderedElementsAre(Pair(24.0f, RangePairs{{10, 20}, {10, 20}})));
}

TEST_F(AXStyleDataTest, AddRange_ConsecutiveRanges) {
  AXStyleData::AddRange(style_data_.text_sizes, 24.0f, 10, 20);
  AXStyleData::AddRange(style_data_.text_sizes, 24.0f, 20, 30);
  EXPECT_THAT(
      *style_data_.text_sizes,
      UnorderedElementsAre(Pair(24.0f, RangePairs{{10, 20}, {20, 30}})));
}

TEST_F(AXStyleDataTest, AddRange_OverlappingRanges) {
  AXStyleData::AddRange(style_data_.text_sizes, 24.0f, 10, 30);
  AXStyleData::AddRange(style_data_.text_sizes, 24.0f, 20, 40);
  EXPECT_THAT(
      *style_data_.text_sizes,
      UnorderedElementsAre(Pair(24.0f, RangePairs{{10, 30}, {20, 40}})));
}

}  // namespace content
