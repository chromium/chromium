// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/length.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"

namespace blink {

namespace {

const PixelsAndPercent ten_px(10,
                              0,
                              /*has_explicit_pixels=*/true,
                              /*has_explicit_percent=*/true);
const PixelsAndPercent twenty_px(20,
                                 0,
                                 /*has_explicit_pixels=*/true,
                                 /*has_explicit_percent=*/true);
const PixelsAndPercent thirty_px(30,
                                 0,
                                 /*has_explicit_pixels=*/true,
                                 /*has_explicit_percent=*/true);
const PixelsAndPercent ten_percent(0,
                                   10,
                                   /*has_explicit_pixels=*/true,
                                   /*has_explicit_percent=*/true);
const PixelsAndPercent twenty_percent(0,
                                      20,
                                      /*has_explicit_pixels=*/true,
                                      /*has_explicit_percent=*/true);
const PixelsAndPercent thirty_percent(0,
                                      30,
                                      /*has_explicit_pixels=*/true,
                                      /*has_explicit_percent=*/true);
const PixelsAndPercent twenty_px_ten_percent(20,
                                             10,
                                             /*has_explicit_pixels=*/true,
                                             /*has_explicit_percent=*/true);

}  // namespace

class LengthTest : public ::testing::Test {
 public:
  using Pointer = scoped_refptr<const CalculationExpressionNode>;

  Pointer PixelsAndPercent(PixelsAndPercent value) {
    return base::MakeRefCounted<CalculationExpressionPixelsAndPercentNode>(
        value);
  }

  Pointer Add(Pointer lhs, Pointer rhs) {
    return base::MakeRefCounted<CalculationExpressionOperationNode>(
        CalculationExpressionOperationNode::Children(
            {std::move(lhs), std::move(rhs)}),
        CalculationOperator::kAdd);
  }

  Pointer Subtract(Pointer lhs, Pointer rhs) {
    return base::MakeRefCounted<CalculationExpressionOperationNode>(
        CalculationExpressionOperationNode::Children(
            {std::move(lhs), std::move(rhs)}),
        CalculationOperator::kSubtract);
  }

  Pointer Multiply(Pointer node, float factor) {
    return base::MakeRefCounted<CalculationExpressionOperationNode>(
        CalculationExpressionOperationNode::Children(
            {std::move(node),
             base::MakeRefCounted<CalculationExpressionNumberNode>(factor)}),
        CalculationOperator::kMultiply);
  }

  Pointer Min(Vector<Pointer>&& operands) {
    return base::MakeRefCounted<CalculationExpressionOperationNode>(
        std::move(operands), CalculationOperator::kMin);
  }

  Pointer Max(Vector<Pointer>&& operands) {
    return base::MakeRefCounted<CalculationExpressionOperationNode>(
        std::move(operands), CalculationOperator::kMax);
  }

  Pointer Clamp(Vector<Pointer>&& operands) {
    return base::MakeRefCounted<CalculationExpressionOperationNode>(
        std::move(operands), CalculationOperator::kClamp);
  }

  Length CreateLength(Pointer expression) {
    return Length(CalculationValue::CreateSimplified(std::move(expression),
                                                     Length::ValueRange::kAll));
  }
};

TEST_F(LengthTest, EvaluateSimpleComparison) {
  // min(10px, 20px)
  {
    Length length = CreateLength(
        Min({PixelsAndPercent(ten_px), PixelsAndPercent(twenty_px)}));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(-200));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(-100));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(0));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(200));
  }

  // min(10%, 20%)
  {
    Length length = CreateLength(
        Min({PixelsAndPercent(ten_percent), PixelsAndPercent(twenty_percent)}));
    EXPECT_EQ(-40.0f, length.GetCalculationValue().Evaluate(-200));
    EXPECT_EQ(-20.0f, length.GetCalculationValue().Evaluate(-100));
    EXPECT_EQ(0.0f, length.GetCalculationValue().Evaluate(0));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(200));
  }

  // min(10px, 10%)
  {
    Length length = CreateLength(
        Min({PixelsAndPercent(ten_px), PixelsAndPercent(twenty_percent)}));
    EXPECT_EQ(-40.0f, length.GetCalculationValue().Evaluate(-200));
    EXPECT_EQ(-20.0f, length.GetCalculationValue().Evaluate(-100));
    EXPECT_EQ(0.0f, length.GetCalculationValue().Evaluate(0));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(200));
  }

  // max(10px, 20px)
  {
    Length length = CreateLength(
        Max({PixelsAndPercent(ten_px), PixelsAndPercent(twenty_px)}));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(-200));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(-100));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(0));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(200));
  }

  // max(10%, 20%)
  {
    Length length = CreateLength(
        Max({PixelsAndPercent(ten_percent), PixelsAndPercent(twenty_percent)}));
    EXPECT_EQ(-20.0f, length.GetCalculationValue().Evaluate(-200));
    EXPECT_EQ(-10.0f, length.GetCalculationValue().Evaluate(-100));
    EXPECT_EQ(0.0f, length.GetCalculationValue().Evaluate(0));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(40.0f, length.GetCalculationValue().Evaluate(200));
  }

  // max(10px, 10%)
  {
    Length length = CreateLength(
        Max({PixelsAndPercent(ten_px), PixelsAndPercent(ten_percent)}));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(-200));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(-100));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(0));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(200));
  }
}

TEST_F(LengthTest, EvaluateNestedComparisons) {
  // max(10px, min(10%, 20px))
  {
    Length length = CreateLength(Max(
        {PixelsAndPercent(ten_px),
         Min({PixelsAndPercent(ten_percent), PixelsAndPercent(twenty_px)})}));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(15.0f, length.GetCalculationValue().Evaluate(150));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(250));
  }

  // max(10%, min(10px, 20%))
  {
    Length length = CreateLength(Max(
        {PixelsAndPercent(ten_percent),
         Min({PixelsAndPercent(ten_px), PixelsAndPercent(twenty_percent)})}));
    EXPECT_EQ(5.0f, length.GetCalculationValue().Evaluate(25));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(75));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(12.5f, length.GetCalculationValue().Evaluate(125));
  }

  // min(max(10px, 10%), 20px)
  {
    Length length = CreateLength(
        Min({Max({PixelsAndPercent(ten_px), PixelsAndPercent(ten_percent)}),
             PixelsAndPercent(twenty_px)}));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(15.0f, length.GetCalculationValue().Evaluate(150));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(250));
  }

  // min(max(10%, 10px), 20%)
  {
    Length length = CreateLength(
        Min({Max({PixelsAndPercent(ten_percent), PixelsAndPercent(ten_px)}),
             PixelsAndPercent(twenty_percent)}));
    EXPECT_EQ(5.0f, length.GetCalculationValue().Evaluate(25));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(75));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(12.5f, length.GetCalculationValue().Evaluate(125));
  }
}

TEST_F(LengthTest, EvaluateAdditive) {
  // min(10%, 10px) + 10px
  {
    Length length = CreateLength(
        Add(Min({PixelsAndPercent(ten_percent), PixelsAndPercent(ten_px)}),
            PixelsAndPercent(ten_px)));
    EXPECT_EQ(15.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(150));
  }

  // min(10%, 10px) - 10px
  {
    Length length = CreateLength(
        Subtract(Min({PixelsAndPercent(ten_percent), PixelsAndPercent(ten_px)}),
                 PixelsAndPercent(ten_px)));
    EXPECT_EQ(-5.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(0.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(0.0f, length.GetCalculationValue().Evaluate(150));
  }

  // 10px + max(10%, 10px)
  {
    Length length = CreateLength(
        Add(PixelsAndPercent(ten_px),
            Max({PixelsAndPercent(ten_percent), PixelsAndPercent(ten_px)})));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(25.0f, length.GetCalculationValue().Evaluate(150));
  }

  // 10px - max(10%, 10px)
  {
    Length length = CreateLength(Subtract(
        PixelsAndPercent(ten_px),
        Max({PixelsAndPercent(ten_percent), PixelsAndPercent(ten_px)})));
    EXPECT_EQ(0.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(0.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(-5.0f, length.GetCalculationValue().Evaluate(150));
  }
}

TEST_F(LengthTest, EvaluateMultiplicative) {
  // min(10px, 10%) * 2
  {
    Length length = CreateLength(Multiply(
        Min({PixelsAndPercent(ten_px), PixelsAndPercent(ten_percent)}), 2));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(150));
  }

  // max(10px, 10%) * 0.5
  {
    Length length = CreateLength(Multiply(
        Max({PixelsAndPercent(ten_px), PixelsAndPercent(ten_percent)}), 0.5));
    EXPECT_EQ(5.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(5.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(200));
  }
}

TEST_F(LengthTest, EvaluateClamp) {
  // clamp(10px, 20px, 30px)
  {
    Length length = CreateLength(
        Clamp({PixelsAndPercent(ten_px), PixelsAndPercent(twenty_px),
               PixelsAndPercent(thirty_px)}));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(150));
  }
  // clamp(20px, 10px, 30px)
  {
    Length length = CreateLength(
        Clamp({PixelsAndPercent(twenty_px), PixelsAndPercent(ten_px),
               PixelsAndPercent(thirty_px)}));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(150));
  }
  // clamp(30px, 10px, 20px)
  {
    Length length = CreateLength(
        Clamp({PixelsAndPercent(thirty_px), PixelsAndPercent(ten_px),
               PixelsAndPercent(twenty_px)}));
    EXPECT_EQ(30.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(30.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(30.0f, length.GetCalculationValue().Evaluate(150));
  }

  // clamp(10%, 20%, 30%)
  {
    Length length = CreateLength(
        Clamp({PixelsAndPercent(ten_percent), PixelsAndPercent(twenty_percent),
               PixelsAndPercent(thirty_percent)}));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(30.0f, length.GetCalculationValue().Evaluate(150));
  }

  // clamp(20%, 10%, 30%)
  {
    Length length = CreateLength(
        Clamp({PixelsAndPercent(twenty_percent), PixelsAndPercent(ten_percent),
               PixelsAndPercent(thirty_percent)}));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(30.0f, length.GetCalculationValue().Evaluate(150));
  }

  // clamp(30%, 10%, 20%)
  {
    Length length = CreateLength(
        Clamp({PixelsAndPercent(thirty_percent), PixelsAndPercent(ten_percent),
               PixelsAndPercent(twenty_percent)}));
    EXPECT_EQ(45.0f, length.GetCalculationValue().Evaluate(150));
    EXPECT_EQ(90.0f, length.GetCalculationValue().Evaluate(300));
    EXPECT_EQ(135.0f, length.GetCalculationValue().Evaluate(450));
  }

  // clamp(20px + 10%, 20%, 30%)
  {
    Length length = CreateLength(Clamp({PixelsAndPercent(twenty_px_ten_percent),
                                        PixelsAndPercent(twenty_percent),
                                        PixelsAndPercent(thirty_percent)}));
    EXPECT_EQ(35.0f, length.GetCalculationValue().Evaluate(150));
    EXPECT_EQ(60.0f, length.GetCalculationValue().Evaluate(300));
    EXPECT_EQ(90.0f, length.GetCalculationValue().Evaluate(450));
  }
}

TEST_F(LengthTest, BlendExpressions) {
  // From: min(10px, 20%)
  // To: max(20px, 10%)
  // Progress: 0.25

  Length from_length = CreateLength(
      Min({PixelsAndPercent(ten_px), PixelsAndPercent(twenty_percent)}));
  Length to_length = CreateLength(
      Max({PixelsAndPercent(twenty_px), PixelsAndPercent(ten_percent)}));
  Length blended = to_length.Blend(from_length, 0.25, Length::ValueRange::kAll);

  EXPECT_EQ(8.75f, blended.GetCalculationValue().Evaluate(25));
  EXPECT_EQ(12.5f, blended.GetCalculationValue().Evaluate(50));
  EXPECT_EQ(12.5f, blended.GetCalculationValue().Evaluate(100));
  EXPECT_EQ(12.5f, blended.GetCalculationValue().Evaluate(200));
  EXPECT_EQ(17.5f, blended.GetCalculationValue().Evaluate(400));
}

TEST_F(LengthTest, ZoomExpression) {
  // Original: min(10px, 10%)
  // Factor: 2.0
  {
    Length original = CreateLength(
        Min({PixelsAndPercent(ten_px), PixelsAndPercent(ten_percent)}));
    Length zoomed = original.Zoom(2);
    EXPECT_EQ(10.0f, zoomed.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(20.0f, zoomed.GetCalculationValue().Evaluate(200));
    EXPECT_EQ(20.0f, zoomed.GetCalculationValue().Evaluate(400));
  }

  // Original: max(10px, 10%)
  // Factor: 0.5
  {
    Length original = CreateLength(
        Max({PixelsAndPercent(ten_px), PixelsAndPercent(ten_percent)}));
    Length zoomed = original.Zoom(0.5);
    EXPECT_EQ(5.0f, zoomed.GetCalculationValue().Evaluate(25));
    EXPECT_EQ(5.0f, zoomed.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(10.0f, zoomed.GetCalculationValue().Evaluate(100));
  }
}

TEST_F(LengthTest, SubtractExpressionFromOneHundredPercent) {
  // min(10px, 20%)
  {
    Length original = CreateLength(
        Min({PixelsAndPercent(ten_px), PixelsAndPercent(twenty_percent)}));
    Length result = original.SubtractFromOneHundredPercent();
    EXPECT_EQ(20.0f, result.GetCalculationValue().Evaluate(25));
    EXPECT_EQ(40.0f, result.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(90.0f, result.GetCalculationValue().Evaluate(100));
  }

  // max(20px, 10%)
  {
    Length original = CreateLength(
        Max({PixelsAndPercent(twenty_px), PixelsAndPercent(ten_percent)}));
    Length result = original.SubtractFromOneHundredPercent();
    EXPECT_EQ(80.0f, result.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(180.0f, result.GetCalculationValue().Evaluate(200));
    EXPECT_EQ(360.0f, result.GetCalculationValue().Evaluate(400));
  }
}

TEST_F(LengthTest, SimplifiedExpressionFromComparisonCreation) {
  // min(10px, 20px, 30px)
  {
    Length original =
        CreateLength(Min({PixelsAndPercent(ten_px), PixelsAndPercent(twenty_px),
                          PixelsAndPercent(thirty_px)}));
    Length zoomed = original.Zoom(1);
    // If it was not simplified, DCHECK fails in
    // CalculationValue::GetPixelsAndPercent.
    auto result = zoomed.GetCalculationValue().GetPixelsAndPercent();
    EXPECT_EQ(10.0f, result.pixels);
  }

  // max(10px, 20px, 30px)
  {
    Length original =
        CreateLength(Max({PixelsAndPercent(ten_px), PixelsAndPercent(twenty_px),
                          PixelsAndPercent(thirty_px)}));
    Length zoomed = original.Zoom(1);
    auto result = zoomed.GetCalculationValue().GetPixelsAndPercent();
    EXPECT_EQ(30.0f, result.pixels);
  }
}

// Non-simplified and simplified CalculationExpressionOperationNode creation
// with CalculationOperator::kMultiply should return the same evaluation result.
TEST_F(LengthTest, MultiplyPixelsAndPercent) {
  // Multiply (20px + 10%) by 2
  Length non_simplified =
      CreateLength(Multiply(PixelsAndPercent(twenty_px_ten_percent), 2));
  const auto& non_simplified_calc_value = non_simplified.GetCalculationValue();
  EXPECT_TRUE(non_simplified_calc_value.IsExpression());
  float result_for_non_simplified =
      non_simplified_calc_value.GetOrCreateExpression()->Evaluate(100, {});
  EXPECT_EQ(60.0f, result_for_non_simplified);

  Length simplified =
      CreateLength(CalculationExpressionOperationNode::CreateSimplified(
          CalculationExpressionOperationNode::Children(
              {PixelsAndPercent(twenty_px_ten_percent),
               base::MakeRefCounted<CalculationExpressionNumberNode>(2)}),
          CalculationOperator::kMultiply));
  const auto& simplified_calc_value = simplified.GetCalculationValue();
  EXPECT_FALSE(simplified_calc_value.IsExpression());
  float result_for_simplified = simplified_calc_value.Evaluate(100);
  EXPECT_EQ(60.0f, result_for_simplified);
}

TEST_F(LengthTest, ZoomToOperation) {
  // Add 10px + 20px
  {
    Length original = CreateLength(
        Add(PixelsAndPercent(ten_px), PixelsAndPercent(twenty_px)));
    Length zoomed = original.Zoom(1);
    // If it was not simplified, DCHECK fails in
    // CalculationValue::GetPixelsAndPercent.
    auto result = zoomed.GetCalculationValue().GetPixelsAndPercent();
    EXPECT_EQ(30.0f, result.pixels);
  }

  // Subtract 20px - 10px
  {
    Length original = CreateLength(
        Subtract(PixelsAndPercent(twenty_px), PixelsAndPercent(ten_px)));
    Length zoomed = original.Zoom(1);
    auto result = zoomed.GetCalculationValue().GetPixelsAndPercent();
    EXPECT_EQ(10.0f, result.pixels);
  }

  // Multiply 30px by 3
  {
    Length original = CreateLength(Multiply(PixelsAndPercent(thirty_px), 3));
    Length zoomed = original.Zoom(1);
    auto result = zoomed.GetCalculationValue().GetPixelsAndPercent();
    EXPECT_EQ(90.0f, result.pixels);
  }

  // min(10px, 20px, 30px) with zoom by 2
  {
    Length original =
        CreateLength(Min({PixelsAndPercent(ten_px), PixelsAndPercent(twenty_px),
                          PixelsAndPercent(thirty_px)}));
    Length zoomed = original.Zoom(2);
    auto result = zoomed.GetCalculationValue().GetPixelsAndPercent();
    EXPECT_EQ(20.0f, result.pixels);
  }

  // max(10px, 20px, 30px) with zoom by 2
  {
    Length original =
        CreateLength(Max({PixelsAndPercent(ten_px), PixelsAndPercent(twenty_px),
                          PixelsAndPercent(thirty_px)}));
    Length zoomed = original.Zoom(2);
    auto result = zoomed.GetCalculationValue().GetPixelsAndPercent();
    EXPECT_EQ(60.0f, result.pixels);
  }

  // clamp(10px, 20px, 30px) with zoom by 2
  {
    Length original = CreateLength(
        Clamp({PixelsAndPercent(ten_px), PixelsAndPercent(twenty_px),
               PixelsAndPercent(thirty_px)}));
    Length zoomed = original.Zoom(2);
    auto result = zoomed.GetCalculationValue().GetPixelsAndPercent();
    EXPECT_EQ(40.0f, result.pixels);
  }
}

TEST_F(LengthTest, Add) {
  // 1px + 1px = 2px
  EXPECT_EQ(2.0f, Length::Fixed(1).Add(Length::Fixed(1)).Pixels());

  // 1px + 0px = 1px
  EXPECT_EQ(1.0f, Length::Fixed(1).Add(Length::Fixed(0)).Pixels());

  // 0px + 1px = 1px
  EXPECT_EQ(1.0f, Length::Fixed(0).Add(Length::Fixed(1)).Pixels());

  // 1% + 1% = 2%
  EXPECT_EQ(2.0f, Length::Percent(1).Add(Length::Percent(1)).Percent());

  // 1% + 0% = 1%
  EXPECT_EQ(1.0f, Length::Percent(1).Add(Length::Percent(0)).Percent());

  // 0% + 1% = 1%
  EXPECT_EQ(1.0f, Length::Percent(0).Add(Length::Percent(1)).Percent());

  // 1px + 10% = calc(1px + 10%) = 2px (for a max_value of 10)
  EXPECT_EQ(2.0f, Length::Fixed(1)
                      .Add(Length::Percent(10))
                      .GetCalculationValue()
                      .Evaluate(10));

  // 10% + 1px = calc(10% + 1px) = 2px (for a max_value of 10)
  EXPECT_EQ(2.0f, Length::Percent(10)
                      .Add(Length::Fixed(1))
                      .GetCalculationValue()
                      .Evaluate(10));

  // 1px + calc(10px * 3) = 31px
  const Length non_simplified =
      CreateLength(Multiply(PixelsAndPercent(ten_px), 3));
  EXPECT_EQ(
      31.0f,
      Length::Fixed(1).Add(non_simplified).GetCalculationValue().Evaluate(123));

  // calc(10px * 3) + 1px = 31px
  EXPECT_EQ(
      31.0f,
      non_simplified.Add(Length::Fixed(1)).GetCalculationValue().Evaluate(123));
}

}  // namespace blink
