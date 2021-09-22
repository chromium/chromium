// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/media_query_exp.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

MediaQueryExpValue IdentValue(CSSValueID id) {
  return MediaQueryExpValue(id);
}

MediaQueryExpValue RatioValue(unsigned numerator, unsigned denominator) {
  return MediaQueryExpValue(numerator, denominator);
}

MediaQueryExpValue PxValue(double value) {
  return MediaQueryExpValue(value, CSSPrimitiveValue::UnitType::kPixels);
}

MediaQueryExpValue EmValue(double value) {
  return MediaQueryExpValue(value, CSSPrimitiveValue::UnitType::kEms);
}

MediaQueryExpValue InvalidValue() {
  return MediaQueryExpValue();
}

MediaQueryExpComparison NoCmp(MediaQueryExpValue v) {
  return MediaQueryExpComparison(v);
}

MediaQueryExpComparison LtCmp(MediaQueryExpValue v) {
  return MediaQueryExpComparison(v, MediaQueryOperator::kLt);
}

MediaQueryExpComparison LeCmp(MediaQueryExpValue v) {
  return MediaQueryExpComparison(v, MediaQueryOperator::kLe);
}

MediaQueryExpComparison GtCmp(MediaQueryExpValue v) {
  return MediaQueryExpComparison(v, MediaQueryOperator::kGt);
}

MediaQueryExpComparison GeCmp(MediaQueryExpValue v) {
  return MediaQueryExpComparison(v, MediaQueryOperator::kGe);
}

MediaQueryExpComparison EqCmp(MediaQueryExpValue v) {
  return MediaQueryExpComparison(v, MediaQueryOperator::kEq);
}

MediaQueryExp LeftExp(String feature, MediaQueryExpComparison cmp) {
  return MediaQueryExp::Create(feature,
                               MediaQueryExpBounds(cmp, NoCmp(InvalidValue())));
}

MediaQueryExp RightExp(String feature, MediaQueryExpComparison cmp) {
  return MediaQueryExp::Create(feature,
                               MediaQueryExpBounds(NoCmp(InvalidValue()), cmp));
}

MediaQueryExp PairExp(String feature,
                      MediaQueryExpComparison left,
                      MediaQueryExpComparison right) {
  return MediaQueryExp::Create(feature, MediaQueryExpBounds(left, right));
}

std::unique_ptr<MediaQueryExpNode> FeatureNode(MediaQueryExp expr) {
  return std::make_unique<MediaQueryFeatureExpNode>(expr);
}

std::unique_ptr<MediaQueryExpNode> NestedNode(
    std::unique_ptr<MediaQueryExpNode> child) {
  return std::make_unique<MediaQueryNestedExpNode>(std::move(child));
}

std::unique_ptr<MediaQueryExpNode> NotNode(
    std::unique_ptr<MediaQueryExpNode> operand) {
  return std::make_unique<MediaQueryNotExpNode>(std::move(operand));
}

std::unique_ptr<MediaQueryExpNode> AndNode(
    std::unique_ptr<MediaQueryExpNode> left,
    std::unique_ptr<MediaQueryExpNode> right) {
  return std::make_unique<MediaQueryAndExpNode>(std::move(left),
                                                std::move(right));
}

std::unique_ptr<MediaQueryExpNode> OrNode(
    std::unique_ptr<MediaQueryExpNode> left,
    std::unique_ptr<MediaQueryExpNode> right) {
  return std::make_unique<MediaQueryOrExpNode>(std::move(left),
                                               std::move(right));
}

}  // namespace

TEST(MediaQueryExpTest, ValuesType) {
  EXPECT_TRUE(IdentValue(CSSValueID::kTop).IsId());
  EXPECT_TRUE(PxValue(10).IsNumeric());
  EXPECT_TRUE(RatioValue(0, 1).IsRatio());
}

TEST(MediaQueryExpTest, ValueEquality) {
  EXPECT_EQ(PxValue(10), PxValue(10));
  EXPECT_EQ(EmValue(10), EmValue(10));
  EXPECT_EQ(IdentValue(CSSValueID::kTop), IdentValue(CSSValueID::kTop));
  EXPECT_EQ(IdentValue(CSSValueID::kTop), IdentValue(CSSValueID::kTop));
  EXPECT_EQ(RatioValue(1, 2), RatioValue(1, 2));

  // Mismatched values:
  EXPECT_NE(PxValue(10), PxValue(20));
  EXPECT_NE(EmValue(20), EmValue(10));
  EXPECT_NE(IdentValue(CSSValueID::kTop), IdentValue(CSSValueID::kLeft));
  EXPECT_NE(RatioValue(16, 9), RatioValue(4, 3));

  // Mismatched unit:
  EXPECT_NE(PxValue(10), EmValue(10));

  // Mismatched types:
  EXPECT_NE(PxValue(10), IdentValue(CSSValueID::kTop));
  EXPECT_NE(PxValue(10), RatioValue(1, 2));

  // Mismatched validity:
  EXPECT_NE(PxValue(10), InvalidValue());
  EXPECT_NE(PxValue(0), InvalidValue());
  EXPECT_NE(RatioValue(0, 1), InvalidValue());
}

TEST(MediaQueryExpTest, ComparisonEquality) {
  auto px1 = PxValue(10.0);
  auto px2 = PxValue(20.0);

  EXPECT_EQ(LtCmp(px1), LtCmp(px1));

  EXPECT_NE(LtCmp(px1), LeCmp(px1));
  EXPECT_NE(LtCmp(px1), LtCmp(px2));
}

TEST(MediaQueryExpTest, BoundaryEquality) {
  auto px1 = PxValue(10.0);
  auto px2 = PxValue(20.0);

  EXPECT_EQ(MediaQueryExpBounds(LtCmp(px1), LeCmp(px1)),
            MediaQueryExpBounds(LtCmp(px1), LeCmp(px1)));

  EXPECT_NE(MediaQueryExpBounds(LtCmp(px1), LeCmp(px1)),
            MediaQueryExpBounds(GtCmp(px1), LeCmp(px1)));
  EXPECT_NE(MediaQueryExpBounds(LtCmp(px1), LeCmp(px1)),
            MediaQueryExpBounds(LtCmp(px1), GeCmp(px1)));
  EXPECT_NE(MediaQueryExpBounds(LtCmp(px1), LeCmp(px2)),
            MediaQueryExpBounds(LtCmp(px1), LeCmp(px1)));
}

TEST(MediaQueryExpTest, ExpEquality) {
  auto px1 = PxValue(10.0);
  auto px2 = PxValue(20.0);

  EXPECT_EQ(LeftExp("width", LtCmp(px1)), LeftExp("width", LtCmp(px1)));

  EXPECT_NE(LeftExp("width", LtCmp(px1)), LeftExp("height", LtCmp(px1)));
  EXPECT_NE(LeftExp("width", LtCmp(px2)), LeftExp("width", LtCmp(px1)));
  EXPECT_NE(LeftExp("width", LtCmp(px1)), RightExp("width", LtCmp(px1)));
  EXPECT_NE(LeftExp("width", LtCmp(px1)), LeftExp("width", GtCmp(px1)));
}

TEST(MediaQueryExpTest, Serialize) {
  // Boolean feature:
  EXPECT_EQ("(color)", RightExp("color", NoCmp(InvalidValue())).Serialize());

  auto px = PxValue(10.0);

  // Plain feature:
  EXPECT_EQ("(width: 10px)", RightExp("width", NoCmp(px)).Serialize());

  // Ranges:
  EXPECT_EQ("(width = 10px)", RightExp("width", EqCmp(px)).Serialize());
  EXPECT_EQ("(width < 10px)", RightExp("width", LtCmp(px)).Serialize());
  EXPECT_EQ("(width <= 10px)", RightExp("width", LeCmp(px)).Serialize());
  EXPECT_EQ("(width > 10px)", RightExp("width", GtCmp(px)).Serialize());
  EXPECT_EQ("(width >= 10px)", RightExp("width", GeCmp(px)).Serialize());

  EXPECT_EQ("(10px = width)", LeftExp("width", EqCmp(px)).Serialize());
  EXPECT_EQ("(10px < width)", LeftExp("width", LtCmp(px)).Serialize());
  EXPECT_EQ("(10px <= width)", LeftExp("width", LeCmp(px)).Serialize());
  EXPECT_EQ("(10px > width)", LeftExp("width", GtCmp(px)).Serialize());
  EXPECT_EQ("(10px >= width)", LeftExp("width", GeCmp(px)).Serialize());

  EXPECT_EQ(
      "(10px < width < 20px)",
      PairExp("width", LtCmp(PxValue(10.0)), LtCmp(PxValue(20.0))).Serialize());
  EXPECT_EQ(
      "(20px > width > 10px)",
      PairExp("width", GtCmp(PxValue(20.0)), GtCmp(PxValue(10.0))).Serialize());
  EXPECT_EQ(
      "(10px <= width <= 20px)",
      PairExp("width", LeCmp(PxValue(10.0)), LeCmp(PxValue(20.0))).Serialize());
  EXPECT_EQ(
      "(20px > width >= 10px)",
      PairExp("width", GtCmp(PxValue(20.0)), GeCmp(PxValue(10.0))).Serialize());
}

TEST(MediaQueryExpTest, SerializeNode) {
  EXPECT_EQ("(width < 10px)",
            FeatureNode(RightExp("width", LtCmp(PxValue(10))))->Serialize());

  EXPECT_EQ(
      "(width < 10px) and (11px >= thing) and (height = 12px)",
      AndNode(FeatureNode(RightExp("width", LtCmp(PxValue(10)))),
              AndNode(FeatureNode(LeftExp("thing", GeCmp(PxValue(11)))),
                      FeatureNode(RightExp("height", EqCmp(PxValue(12))))))
          ->Serialize());

  // Same as previous, but with 'or' instead:
  EXPECT_EQ("(width < 10px) or (11px >= thing) or (height = 12px)",
            OrNode(FeatureNode(RightExp("width", LtCmp(PxValue(10)))),
                   OrNode(FeatureNode(LeftExp("thing", GeCmp(PxValue(11)))),
                          FeatureNode(RightExp("height", EqCmp(PxValue(12))))))
                ->Serialize());

  EXPECT_EQ(
      "not (width < 10px)",
      NotNode(FeatureNode(RightExp("width", LtCmp(PxValue(10)))))->Serialize());

  EXPECT_EQ("((width < 10px))",
            NestedNode(FeatureNode(RightExp("width", LtCmp(PxValue(10)))))
                ->Serialize());
  EXPECT_EQ(
      "(((width < 10px)))",
      NestedNode(NestedNode(FeatureNode(RightExp("width", LtCmp(PxValue(10))))))
          ->Serialize());

  EXPECT_EQ("not ((11px >= thing) and (height = 12px))",
            NotNode(NestedNode(AndNode(
                        FeatureNode(LeftExp("thing", GeCmp(PxValue(11)))),
                        FeatureNode(RightExp("height", EqCmp(PxValue(12)))))))
                ->Serialize());
}

}  // namespace blink
