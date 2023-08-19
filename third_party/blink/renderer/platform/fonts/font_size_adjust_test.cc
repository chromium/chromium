// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_size_adjust.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(FontSizeAdjustTest, HashingAndComparison) {
  EXPECT_EQ(FontSizeAdjust(),
            FontSizeAdjust(FontSizeAdjust::kFontSizeAdjustNone));
  EXPECT_EQ(FontSizeAdjust().GetHash(),
            FontSizeAdjust(FontSizeAdjust::kFontSizeAdjustNone).GetHash());

  EXPECT_EQ(FontSizeAdjust(0.5), FontSizeAdjust(0.5));
  EXPECT_EQ(FontSizeAdjust(0.5).GetHash(), FontSizeAdjust(0.5).GetHash());

  EXPECT_EQ(FontSizeAdjust(0.5),
            FontSizeAdjust(0.5, FontSizeAdjust::Metric::kExHeight));
  EXPECT_EQ(FontSizeAdjust(0.5).GetHash(),
            FontSizeAdjust(0.5, FontSizeAdjust::Metric::kExHeight).GetHash());

  EXPECT_EQ(FontSizeAdjust(0.5), FontSizeAdjust(0.5, false));
  EXPECT_EQ(FontSizeAdjust(0.5).GetHash(),
            FontSizeAdjust(0.5, false).GetHash());

  EXPECT_EQ(FontSizeAdjust(0.5),
            FontSizeAdjust(0.5, FontSizeAdjust::Metric::kExHeight, false));
  EXPECT_EQ(
      FontSizeAdjust(0.5).GetHash(),
      FontSizeAdjust(0.5, FontSizeAdjust::Metric::kExHeight, false).GetHash());

  EXPECT_EQ(FontSizeAdjust(0.5, false),
            FontSizeAdjust(0.5, FontSizeAdjust::Metric::kExHeight));
  EXPECT_EQ(FontSizeAdjust(0.5, false).GetHash(),
            FontSizeAdjust(0.5, FontSizeAdjust::Metric::kExHeight).GetHash());

  EXPECT_NE(FontSizeAdjust(), FontSizeAdjust(0.0));
  EXPECT_NE(FontSizeAdjust().GetHash(), FontSizeAdjust(0.0).GetHash());

  EXPECT_NE(FontSizeAdjust(0.5), FontSizeAdjust(1.5));
  EXPECT_NE(FontSizeAdjust(0.5).GetHash(), FontSizeAdjust(1.5).GetHash());

  EXPECT_NE(FontSizeAdjust(0.5), FontSizeAdjust(0.5, true));
  EXPECT_NE(FontSizeAdjust(0.5).GetHash(), FontSizeAdjust(0.5, true).GetHash());

  EXPECT_NE(FontSizeAdjust(0.5, FontSizeAdjust::Metric::kExHeight),
            FontSizeAdjust(0.5, FontSizeAdjust::Metric::kCapHeight));
  EXPECT_NE(FontSizeAdjust(0.5, FontSizeAdjust::Metric::kExHeight).GetHash(),
            FontSizeAdjust(0.5, FontSizeAdjust::Metric::kCapHeight).GetHash());

  EXPECT_NE(FontSizeAdjust(0.5, FontSizeAdjust::Metric::kExHeight),
            FontSizeAdjust(0.5, FontSizeAdjust::Metric::kCapHeight));
  EXPECT_NE(FontSizeAdjust(0.5, FontSizeAdjust::Metric::kExHeight).GetHash(),
            FontSizeAdjust(0.5, FontSizeAdjust::Metric::kCapHeight).GetHash());

  EXPECT_NE(FontSizeAdjust(0.5, FontSizeAdjust::Metric::kCapHeight),
            FontSizeAdjust(1.5, FontSizeAdjust::Metric::kCapHeight));
  EXPECT_NE(FontSizeAdjust(0.5, FontSizeAdjust::Metric::kCapHeight).GetHash(),
            FontSizeAdjust(1.5, FontSizeAdjust::Metric::kCapHeight).GetHash());

  EXPECT_NE(FontSizeAdjust(0.5, FontSizeAdjust::Metric::kCapHeight),
            FontSizeAdjust(0.5, FontSizeAdjust::Metric::kCapHeight, true));
  EXPECT_NE(
      FontSizeAdjust(0.5, FontSizeAdjust::Metric::kCapHeight).GetHash(),
      FontSizeAdjust(0.5, FontSizeAdjust::Metric::kCapHeight, true).GetHash());
}

TEST(FontSizeAdjustTest, Serialization) {
  EXPECT_EQ("none", FontSizeAdjust().ToString());
  EXPECT_EQ("0.5", FontSizeAdjust(0.5).ToString());
  EXPECT_EQ("0.5",
            FontSizeAdjust(0.5, FontSizeAdjust::Metric::kExHeight).ToString());
  EXPECT_EQ("cap-height 0.5",
            FontSizeAdjust(0.5, FontSizeAdjust::Metric::kCapHeight).ToString());
  EXPECT_EQ("ch-width 0.5",
            FontSizeAdjust(0.5, FontSizeAdjust::Metric::kChWidth).ToString());
  EXPECT_EQ("ic-width 0.5",
            FontSizeAdjust(0.5, FontSizeAdjust::Metric::kIcWidth).ToString());

  EXPECT_EQ("from-font", FontSizeAdjust(0.5, true).ToString());
  EXPECT_EQ(
      "from-font",
      FontSizeAdjust(0.5, FontSizeAdjust::Metric::kExHeight, true).ToString());
  EXPECT_EQ(
      "cap-height from-font",
      FontSizeAdjust(0.5, FontSizeAdjust::Metric::kCapHeight, true).ToString());
  EXPECT_EQ(
      "ch-width from-font",
      FontSizeAdjust(0.5, FontSizeAdjust::Metric::kChWidth, true).ToString());
  EXPECT_EQ(
      "ic-width from-font",
      FontSizeAdjust(0.5, FontSizeAdjust::Metric::kIcWidth, true).ToString());

  EXPECT_NE("none", FontSizeAdjust(0.0).ToString());
  EXPECT_NE("ex-height 0.5", FontSizeAdjust(0.5).ToString());
  EXPECT_NE("cap-height 0.5",
            FontSizeAdjust(0.5, FontSizeAdjust::Metric::kChWidth).ToString());
  EXPECT_NE("cap-height 1.5",
            FontSizeAdjust(0.5, FontSizeAdjust::Metric::kCapHeight).ToString());

  EXPECT_NE("0.5", FontSizeAdjust(0.5, true).ToString());
  EXPECT_NE(
      "0.5",
      FontSizeAdjust(0.5, FontSizeAdjust::Metric::kExHeight, true).ToString());
  EXPECT_NE(
      "ex-height 0.5",
      FontSizeAdjust(0.5, FontSizeAdjust::Metric::kExHeight, true).ToString());
  EXPECT_NE(
      "cap-height 0.5",
      FontSizeAdjust(0.5, FontSizeAdjust::Metric::kChWidth, true).ToString());
  EXPECT_NE(
      "cap-height 1.5",
      FontSizeAdjust(1.5, FontSizeAdjust::Metric::kCapHeight, true).ToString());
}

}  // namespace blink
