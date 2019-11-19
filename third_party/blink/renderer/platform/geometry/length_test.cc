// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/length.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"

namespace blink {

namespace {

const PixelsAndPercent ten_px(10, 0);
const PixelsAndPercent twenty_px(20, 0);
const PixelsAndPercent ten_percent(0, 10);
const PixelsAndPercent twenty_percent(0, 20);

}  // namespace

class LengthTest : public ::testing::Test {
 public:
  using Pointer = scoped_refptr<const CalculationExpressionNode>;

  Pointer Leaf(PixelsAndPercent value) {
    return base::MakeRefCounted<CalculationExpressionLeafNode>(value);
  }

  Pointer Add(Pointer lhs, Pointer rhs) {
    return base::MakeRefCounted<CalculationExpressionAdditiveNode>(
        std::move(lhs), std::move(rhs),
        CalculationExpressionAdditiveNode::Type::kAdd);
  }

  Pointer Subtract(Pointer lhs, Pointer rhs) {
    return base::MakeRefCounted<CalculationExpressionAdditiveNode>(
        std::move(lhs), std::move(rhs),
        CalculationExpressionAdditiveNode::Type::kSubtract);
  }

  Pointer Multiply(Pointer node, float factor) {
    return base::MakeRefCounted<CalculationExpressionMultiplicationNode>(
        std::move(node), factor);
  }

  Pointer Min(Pointer op1, Pointer op2) {
    Vector<Pointer> operands;
    operands.push_back(std::move(op1));
    operands.push_back(std::move(op2));
    return base::MakeRefCounted<CalculationExpressionComparisonNode>(
        std::move(operands), CalculationExpressionComparisonNode::Type::kMin);
  }

  Pointer Max(Pointer op1, Pointer op2) {
    Vector<Pointer> operands;
    operands.push_back(std::move(op1));
    operands.push_back(std::move(op2));
    return base::MakeRefCounted<CalculationExpressionComparisonNode>(
        std::move(operands), CalculationExpressionComparisonNode::Type::kMax);
  }

  Length CreateLength(Pointer expression) {
    return Length(CalculationValue::CreateSimplified(std::move(expression),
                                                     kValueRangeAll));
  }
};

TEST_F(LengthTest, EvaluateSimpleComparison) {
  // min(10px, 20px)
  {
    Length length = CreateLength(Min(Leaf(ten_px), Leaf(twenty_px)));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(-200));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(-100));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(0));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(200));
  }

  // min(10%, 20%)
  {
    Length length = CreateLength(Min(Leaf(ten_percent), Leaf(twenty_percent)));
    EXPECT_EQ(-40.0f, length.GetCalculationValue().Evaluate(-200));
    EXPECT_EQ(-20.0f, length.GetCalculationValue().Evaluate(-100));
    EXPECT_EQ(0.0f, length.GetCalculationValue().Evaluate(0));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(200));
  }

  // min(10px, 10%)
  {
    Length length = CreateLength(Min(Leaf(ten_px), Leaf(twenty_percent)));
    EXPECT_EQ(-40.0f, length.GetCalculationValue().Evaluate(-200));
    EXPECT_EQ(-20.0f, length.GetCalculationValue().Evaluate(-100));
    EXPECT_EQ(0.0f, length.GetCalculationValue().Evaluate(0));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(200));
  }

  // max(10px, 20px)
  {
    Length length = CreateLength(Max(Leaf(ten_px), Leaf(twenty_px)));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(-200));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(-100));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(0));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(200));
  }

  // max(10%, 20%)
  {
    Length length = CreateLength(Max(Leaf(ten_percent), Leaf(twenty_percent)));
    EXPECT_EQ(-20.0f, length.GetCalculationValue().Evaluate(-200));
    EXPECT_EQ(-10.0f, length.GetCalculationValue().Evaluate(-100));
    EXPECT_EQ(0.0f, length.GetCalculationValue().Evaluate(0));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(40.0f, length.GetCalculationValue().Evaluate(200));
  }

  // max(10px, 10%)
  {
    Length length = CreateLength(Max(Leaf(ten_px), Leaf(ten_percent)));
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
    Length length = CreateLength(
        Max(Leaf(ten_px), Min(Leaf(ten_percent), Leaf(twenty_px))));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(15.0f, length.GetCalculationValue().Evaluate(150));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(250));
  }

  // max(10%, min(10px, 20%))
  {
    Length length = CreateLength(
        Max(Leaf(ten_percent), Min(Leaf(ten_px), Leaf(twenty_percent))));
    EXPECT_EQ(5.0f, length.GetCalculationValue().Evaluate(25));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(75));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(12.5f, length.GetCalculationValue().Evaluate(125));
  }

  // min(max(10px, 10%), 20px)
  {
    Length length = CreateLength(
        Min(Max(Leaf(ten_px), Leaf(ten_percent)), Leaf(twenty_px)));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(15.0f, length.GetCalculationValue().Evaluate(150));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(250));
  }

  // min(max(10%, 10px), 20%)
  {
    Length length = CreateLength(
        Min(Max(Leaf(ten_percent), Leaf(ten_px)), Leaf(twenty_percent)));
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
    Length length =
        CreateLength(Add(Min(Leaf(ten_percent), Leaf(ten_px)), Leaf(ten_px)));
    EXPECT_EQ(15.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(150));
  }

  // min(10%, 10px) - 10px
  {
    Length length = CreateLength(
        Subtract(Min(Leaf(ten_percent), Leaf(ten_px)), Leaf(ten_px)));
    EXPECT_EQ(-5.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(0.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(0.0f, length.GetCalculationValue().Evaluate(150));
  }

  // 10px + max(10%, 10px)
  {
    Length length =
        CreateLength(Add(Leaf(ten_px), Max(Leaf(ten_percent), Leaf(ten_px))));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(25.0f, length.GetCalculationValue().Evaluate(150));
  }

  // 10px - max(10%, 10px)
  {
    Length length = CreateLength(
        Subtract(Leaf(ten_px), Max(Leaf(ten_percent), Leaf(ten_px))));
    EXPECT_EQ(0.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(0.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(-5.0f, length.GetCalculationValue().Evaluate(150));
  }
}

TEST_F(LengthTest, EvaluateMultiplicative) {
  // min(10px, 10%) * 2
  {
    Length length =
        CreateLength(Multiply(Min(Leaf(ten_px), Leaf(ten_percent)), 2));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(20.0f, length.GetCalculationValue().Evaluate(150));
  }

  // max(10px, 10%) * 0.5
  {
    Length length =
        CreateLength(Multiply(Max(Leaf(ten_px), Leaf(ten_percent)), 0.5));
    EXPECT_EQ(5.0f, length.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(5.0f, length.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(10.0f, length.GetCalculationValue().Evaluate(200));
  }
}

TEST_F(LengthTest, BlendExpressions) {
  // From: min(10px, 20%)
  // To: max(20px, 10%)
  // Progress: 0.25

  Length from_length = CreateLength(Min(Leaf(ten_px), Leaf(twenty_percent)));
  Length to_length = CreateLength(Max(Leaf(twenty_px), Leaf(ten_percent)));
  Length blended = to_length.Blend(from_length, 0.25, kValueRangeAll);

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
    Length original = CreateLength(Min(Leaf(ten_px), Leaf(ten_percent)));
    Length zoomed = original.Zoom(2);
    EXPECT_EQ(10.0f, zoomed.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(20.0f, zoomed.GetCalculationValue().Evaluate(200));
    EXPECT_EQ(20.0f, zoomed.GetCalculationValue().Evaluate(400));
  }

  // Original: max(10px, 10%)
  // Factor: 0.5
  {
    Length original = CreateLength(Max(Leaf(ten_px), Leaf(ten_percent)));
    Length zoomed = original.Zoom(0.5);
    EXPECT_EQ(5.0f, zoomed.GetCalculationValue().Evaluate(25));
    EXPECT_EQ(5.0f, zoomed.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(10.0f, zoomed.GetCalculationValue().Evaluate(100));
  }
}

TEST_F(LengthTest, SubtractExpressionFromOneHundredPercent) {
  // min(10px, 20%)
  {
    Length original = CreateLength(Min(Leaf(ten_px), Leaf(twenty_percent)));
    Length result = original.SubtractFromOneHundredPercent();
    EXPECT_EQ(20.0f, result.GetCalculationValue().Evaluate(25));
    EXPECT_EQ(40.0f, result.GetCalculationValue().Evaluate(50));
    EXPECT_EQ(90.0f, result.GetCalculationValue().Evaluate(100));
  }

  // max(20px, 10%)
  {
    Length original = CreateLength(Max(Leaf(twenty_px), Leaf(ten_percent)));
    Length result = original.SubtractFromOneHundredPercent();
    EXPECT_EQ(80.0f, result.GetCalculationValue().Evaluate(100));
    EXPECT_EQ(180.0f, result.GetCalculationValue().Evaluate(200));
    EXPECT_EQ(360.0f, result.GetCalculationValue().Evaluate(400));
  }
}

}  // namespace blink
