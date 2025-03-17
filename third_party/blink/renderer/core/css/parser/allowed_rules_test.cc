// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/allowed_rules.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(AllowedRulesTest, QualifiedRuleStyle) {
  AllowedRules allowed = {QualifiedRuleType::kStyle};

  EXPECT_TRUE(allowed.Has(QualifiedRuleType::kStyle));
  EXPECT_FALSE(allowed.Has(QualifiedRuleType::kKeyframe));
  EXPECT_FALSE(allowed.Has(CSSAtRuleID::kCSSAtRuleMedia));

  allowed.Remove(QualifiedRuleType::kStyle);
  EXPECT_EQ(AllowedRules(), allowed);
}

TEST(AllowedRulesTest, QualifiedRuleKeyframe) {
  AllowedRules allowed = {QualifiedRuleType::kKeyframe};

  EXPECT_FALSE(allowed.Has(QualifiedRuleType::kStyle));
  EXPECT_TRUE(allowed.Has(QualifiedRuleType::kKeyframe));
  EXPECT_FALSE(allowed.Has(CSSAtRuleID::kCSSAtRuleMedia));

  allowed.Remove(QualifiedRuleType::kKeyframe);
  EXPECT_EQ(AllowedRules(), allowed);
}

TEST(AllowedRulesTest, QualifiedRuleMultiple) {
  AllowedRules allowed = {QualifiedRuleType::kStyle,
                          QualifiedRuleType::kKeyframe};

  EXPECT_TRUE(allowed.Has(QualifiedRuleType::kStyle));
  EXPECT_TRUE(allowed.Has(QualifiedRuleType::kKeyframe));
  EXPECT_FALSE(allowed.Has(CSSAtRuleID::kCSSAtRuleMedia));

  allowed.Remove(QualifiedRuleType::kStyle);
  EXPECT_FALSE(allowed.Has(QualifiedRuleType::kStyle));
  EXPECT_TRUE(allowed.Has(QualifiedRuleType::kKeyframe));
  allowed.Remove(QualifiedRuleType::kKeyframe);
  EXPECT_EQ(AllowedRules(), allowed);
}

TEST(AllowedRulesTest, InitNone) {
  AllowedRules allowed;

  for (int i = 0; i < static_cast<int>(QualifiedRuleType::kCount); ++i) {
    EXPECT_FALSE(allowed.Has(static_cast<QualifiedRuleType>(i)));
  }

  for (int i = 0; i < static_cast<int>(CSSAtRuleID::kCount); ++i) {
    EXPECT_FALSE(allowed.Has(static_cast<CSSAtRuleID>(i)));
  }
}

TEST(AllowedRulesTest, CSSAtRuleID) {
  AllowedRules allowed = {CSSAtRuleID::kCSSAtRuleViewTransition};

  EXPECT_TRUE(allowed.Has(CSSAtRuleID::kCSSAtRuleViewTransition));
  EXPECT_FALSE(allowed.Has(QualifiedRuleType::kStyle));
  EXPECT_FALSE(allowed.Has(QualifiedRuleType::kKeyframe));
  EXPECT_FALSE(allowed.Has(CSSAtRuleID::kCSSAtRuleMedia));

  allowed.Remove(CSSAtRuleID::kCSSAtRuleViewTransition);
  EXPECT_EQ(AllowedRules(), allowed);
}

TEST(AllowedRulesTest, CSSAtRuleIDRuleMultiple) {
  AllowedRules allowed = {CSSAtRuleID::kCSSAtRuleMedia,
                          CSSAtRuleID::kCSSAtRuleSupports};

  EXPECT_TRUE(allowed.Has(CSSAtRuleID::kCSSAtRuleMedia));
  EXPECT_TRUE(allowed.Has(CSSAtRuleID::kCSSAtRuleSupports));
  EXPECT_FALSE(allowed.Has(QualifiedRuleType::kStyle));
  EXPECT_FALSE(allowed.Has(QualifiedRuleType::kKeyframe));
  EXPECT_FALSE(allowed.Has(CSSAtRuleID::kCSSAtRuleContainer));

  allowed.Remove(CSSAtRuleID::kCSSAtRuleMedia);
  EXPECT_FALSE(allowed.Has(CSSAtRuleID::kCSSAtRuleMedia));
  EXPECT_TRUE(allowed.Has(CSSAtRuleID::kCSSAtRuleSupports));

  allowed.Remove(CSSAtRuleID::kCSSAtRuleSupports);
  EXPECT_FALSE(allowed.Has(CSSAtRuleID::kCSSAtRuleMedia));
  EXPECT_FALSE(allowed.Has(CSSAtRuleID::kCSSAtRuleSupports));

  EXPECT_EQ(AllowedRules(), allowed);
}

TEST(AllowedRulesTest, Mixed) {
  AllowedRules allowed = AllowedRules{QualifiedRuleType::kStyle} |
                         AllowedRules{CSSAtRuleID::kCSSAtRuleMedia,
                                      CSSAtRuleID::kCSSAtRuleSupports};

  EXPECT_TRUE(allowed.Has(QualifiedRuleType::kStyle));
  EXPECT_FALSE(allowed.Has(QualifiedRuleType::kKeyframe));
  EXPECT_TRUE(allowed.Has(CSSAtRuleID::kCSSAtRuleMedia));
  EXPECT_TRUE(allowed.Has(CSSAtRuleID::kCSSAtRuleSupports));
  EXPECT_FALSE(allowed.Has(CSSAtRuleID::kCSSAtRuleContainer));

  allowed.Remove(CSSAtRuleID::kCSSAtRuleMedia);
  EXPECT_TRUE(allowed.Has(QualifiedRuleType::kStyle));
  EXPECT_FALSE(allowed.Has(QualifiedRuleType::kKeyframe));
  EXPECT_FALSE(allowed.Has(CSSAtRuleID::kCSSAtRuleMedia));
  EXPECT_TRUE(allowed.Has(CSSAtRuleID::kCSSAtRuleSupports));
  EXPECT_FALSE(allowed.Has(CSSAtRuleID::kCSSAtRuleContainer));

  allowed.Remove(QualifiedRuleType::kStyle);
  EXPECT_FALSE(allowed.Has(QualifiedRuleType::kStyle));
  EXPECT_FALSE(allowed.Has(QualifiedRuleType::kKeyframe));
  EXPECT_FALSE(allowed.Has(CSSAtRuleID::kCSSAtRuleMedia));
  EXPECT_TRUE(allowed.Has(CSSAtRuleID::kCSSAtRuleSupports));
  EXPECT_FALSE(allowed.Has(CSSAtRuleID::kCSSAtRuleContainer));

  allowed.Remove(CSSAtRuleID::kCSSAtRuleSupports);
  EXPECT_FALSE(allowed.Has(QualifiedRuleType::kStyle));
  EXPECT_FALSE(allowed.Has(QualifiedRuleType::kKeyframe));
  EXPECT_FALSE(allowed.Has(CSSAtRuleID::kCSSAtRuleMedia));
  EXPECT_FALSE(allowed.Has(CSSAtRuleID::kCSSAtRuleSupports));
  EXPECT_FALSE(allowed.Has(CSSAtRuleID::kCSSAtRuleContainer));

  EXPECT_EQ(AllowedRules(), allowed);
}

}  // namespace blink
