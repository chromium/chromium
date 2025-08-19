// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/evaluation_input.h"

namespace blink {

namespace {

const CalculationExpressionOperationNode* BuildOperationNode(
    Vector<float> numbers,
    CalculationOperator op) {
  CalculationExpressionOperationNode::Children operands;
  for (float number : numbers) {
    const CalculationExpressionNumberNode* operand =
        MakeGarbageCollected<CalculationExpressionNumberNode>(number);
    operands.push_back(operand);
  }
  return MakeGarbageCollected<CalculationExpressionOperationNode>(
      std::move(operands), op);
}

}  // namespace

TEST(CalculationExpressionOperationNodeTest, Comparison) {
  const CalculationExpressionOperationNode* operation1 =
      BuildOperationNode({13.f, 17.f}, CalculationOperator::kMax);
  const CalculationExpressionOperationNode* operation2 =
      BuildOperationNode({17.f, 13.f}, CalculationOperator::kMax);
  const CalculationExpressionOperationNode* operation3 =
      BuildOperationNode({17.f, 13.f}, CalculationOperator::kMax);

  EXPECT_EQ(operation1->Evaluate(FLT_MAX, {}),
            operation2->Evaluate(FLT_MAX, {}));
  EXPECT_EQ(*operation2, *operation3);
}

TEST(CalculationExpressionOperationNodeTest, SteppedValueFunctions) {
  const CalculationExpressionOperationNode* operation_nearest_1_1 =
      BuildOperationNode({1, 1}, CalculationOperator::kRoundNearest);
  const CalculationExpressionOperationNode* operation_mod_1_1 =
      BuildOperationNode({1, 1}, CalculationOperator::kMod);
  const CalculationExpressionOperationNode* operation_rem_1_1 =
      BuildOperationNode({1, 1}, CalculationOperator::kRem);
  const CalculationExpressionOperationNode* operation_mod_18_5 =
      BuildOperationNode({18, 5}, CalculationOperator::kMod);
  const CalculationExpressionOperationNode* operation_mod_17_5 =
      BuildOperationNode({17, 5}, CalculationOperator::kMod);

  CalculationExpressionOperationNode::Children operands_rem_two_mods;
  operands_rem_two_mods.push_back(operation_mod_18_5);
  operands_rem_two_mods.push_back(operation_mod_17_5);
  const CalculationExpressionOperationNode* operation_rem_two_mods =
      MakeGarbageCollected<CalculationExpressionOperationNode>(
          std::move(operands_rem_two_mods), CalculationOperator::kRem);

  EXPECT_EQ(operation_nearest_1_1->Evaluate(FLT_MAX, {}), 1.f);
  EXPECT_EQ(operation_mod_1_1->Evaluate(FLT_MAX, {}), 0.f);
  EXPECT_EQ(operation_rem_1_1->Evaluate(FLT_MAX, {}), 0.f);
  EXPECT_EQ(operation_rem_two_mods->Evaluate(FLT_MAX, {}), 1.f);
}

TEST(CalculationExpressionOperationNodeTest, ExponentialFunctions) {
  EXPECT_EQ(BuildOperationNode({3.f, 4.f}, CalculationOperator::kHypot)
                ->Evaluate(FLT_MAX, {}),
            5.f);
  EXPECT_EQ(BuildOperationNode({3e37f, 4e37f}, CalculationOperator::kHypot)
                ->Evaluate(FLT_MAX, {}),
            5e37f);
  EXPECT_EQ(BuildOperationNode({8e-46f, 15e-46f}, CalculationOperator::kHypot)
                ->Evaluate(FLT_MAX, {}),
            17e-46f);
  EXPECT_EQ(
      BuildOperationNode({6e37f, 6e37f, 17e37}, CalculationOperator::kHypot)
          ->Evaluate(FLT_MAX, {}),
      19e37f);
  EXPECT_EQ(BuildOperationNode({-3.f, 4.f}, CalculationOperator::kHypot)
                ->Evaluate(FLT_MAX, {}),
            5.f);
  EXPECT_EQ(BuildOperationNode({-3.f, -4.f}, CalculationOperator::kHypot)
                ->Evaluate(FLT_MAX, {}),
            5.f);
  EXPECT_EQ(BuildOperationNode({-0.f, +0.f}, CalculationOperator::kHypot)
                ->Evaluate(FLT_MAX, {}),
            +0.f);
  EXPECT_EQ(
      BuildOperationNode({6e37f, -6e37f, -17e37}, CalculationOperator::kHypot)
          ->Evaluate(FLT_MAX, {}),
      19e37f);
}

TEST(CalculationExpressionOperationNodeTest,
     SignRelatedFunctionsPixelsAndPercent) {
  const CalculationExpressionNode* pixels_and_percent_node =
      MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
          PixelsAndPercent(-100.0f, -100.0f, /*has_explicit_pixels=*/true,
                           /*has_explicit_percent=*/true));
  CalculationExpressionOperationNode::Children children;
  children.push_back(pixels_and_percent_node);
  const CalculationExpressionNode* pixels_and_percent_operation_abs =
      CalculationExpressionOperationNode::CreateSimplified(
          std::move(children), CalculationOperator::kAbs);
  EXPECT_TRUE(pixels_and_percent_operation_abs->IsOperation());
  EXPECT_EQ(pixels_and_percent_operation_abs->Evaluate(100.0f, {}), 200.0f);
}

TEST(CalculationExpressionOperationNodeTest,
     SignRelatedFunctionsPixelsAndZeroPercent) {
  const CalculationExpressionNode* pixels_and_zero_percent_node =
      MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
          PixelsAndPercent(-100.0f, 0.0f, /*has_explicit_pixels=*/true,
                           /*has_explicit_percent=*/false));
  CalculationExpressionOperationNode::Children children;
  children.push_back(pixels_and_zero_percent_node);
  const CalculationExpressionNode* pixels_and_zero_percent_operation_sign =
      CalculationExpressionOperationNode::CreateSimplified(
          std::move(children), CalculationOperator::kSign);
  EXPECT_TRUE(pixels_and_zero_percent_operation_sign->IsNumber());
  EXPECT_EQ(pixels_and_zero_percent_operation_sign->Evaluate(FLT_MAX, {}),
            -1.0f);
}

TEST(CalculationExpressionOperationNodeTest, SignRelatedFunctionsPixelsOnly) {
  const CalculationExpressionNode* pixels_node =
      MakeGarbageCollected<CalculationExpressionNumberNode>(-0.0f);
  CalculationExpressionOperationNode::Children children;
  children.push_back(pixels_node);
  const CalculationExpressionNode* pixels_operation_sign =
      CalculationExpressionOperationNode::CreateSimplified(
          std::move(children), CalculationOperator::kSign);
  EXPECT_TRUE(pixels_operation_sign->IsOperation());
  EXPECT_TRUE(std::signbit(pixels_operation_sign->Evaluate(FLT_MAX, {})));
}

TEST(CalculationExpressionOperationNodeTest, SignRelatedFunctions) {
  const CalculationExpressionOperationNode* operation_abs_1 =
      BuildOperationNode({1.0f}, CalculationOperator::kAbs);
  const CalculationExpressionOperationNode* operation_abs_minus_1 =
      BuildOperationNode({-1.0f}, CalculationOperator::kAbs);
  const CalculationExpressionOperationNode* operation_abs_minus_0 =
      BuildOperationNode({-0.0f}, CalculationOperator::kAbs);
  const CalculationExpressionOperationNode* operation_sign_1 =
      BuildOperationNode({1.0f}, CalculationOperator::kSign);
  const CalculationExpressionOperationNode* operation_sign_minus_1 =
      BuildOperationNode({-1.0f}, CalculationOperator::kSign);
  const CalculationExpressionOperationNode* operation_sign_0 =
      BuildOperationNode({0.0f}, CalculationOperator::kSign);
  const CalculationExpressionOperationNode* operation_sign_minus_0 =
      BuildOperationNode({-0.0f}, CalculationOperator::kSign);

  EXPECT_EQ(operation_abs_1->Evaluate(FLT_MAX, {}), 1.0f);
  EXPECT_EQ(operation_abs_minus_1->Evaluate(FLT_MAX, {}), 1.0f);
  EXPECT_EQ(operation_abs_minus_0->Evaluate(FLT_MAX, {}), 0.0f);
  EXPECT_EQ(operation_sign_1->Evaluate(FLT_MAX, {}), 1.0f);
  EXPECT_EQ(operation_sign_minus_1->Evaluate(FLT_MAX, {}), -1.0f);
  EXPECT_EQ(operation_sign_0->Evaluate(FLT_MAX, {}), 0.0f);
  EXPECT_TRUE(std::signbit(operation_sign_minus_0->Evaluate(FLT_MAX, {})));
}

TEST(CalculationExpressionOperationNodeTest, ExplicitPixelsAndPercent) {
  const CalculationExpressionNode* node_1 =
      MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
          PixelsAndPercent(0.0f, -100.0f, /*has_explicit_pixels=*/false,
                           /*has_explicit_percent=*/true));
  const CalculationExpressionNode* node_2 =
      MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
          PixelsAndPercent(100.0f));
  const auto* operation_node =
      CalculationExpressionOperationNode::CreateSimplified(
          {node_1, node_2}, CalculationOperator::kAdd);
  const auto* pixels_and_percent_node =
      DynamicTo<CalculationExpressionPixelsAndPercentNode>(*operation_node);
  EXPECT_TRUE(operation_node->IsPixelsAndPercent());
  EXPECT_TRUE(pixels_and_percent_node->HasExplicitPixels());
  EXPECT_TRUE(pixels_and_percent_node->HasExplicitPercent());
}

TEST(CalculationExpressionOperationNodeTest, NonExplicitPixelsAndPercent) {
  const CalculationExpressionNode* node_1 =
      MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
          PixelsAndPercent(10.0f));
  const CalculationExpressionNode* node_2 =
      MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
          PixelsAndPercent(100.0f));
  const auto* operation_node =
      CalculationExpressionOperationNode::CreateSimplified(
          {node_1, node_2}, CalculationOperator::kAdd);
  const auto* pixels_and_percent_node =
      DynamicTo<CalculationExpressionPixelsAndPercentNode>(*operation_node);
  EXPECT_TRUE(operation_node->IsPixelsAndPercent());
  EXPECT_TRUE(pixels_and_percent_node->HasExplicitPixels());
  EXPECT_FALSE(pixels_and_percent_node->HasExplicitPercent());
}

TEST(CalculationExpressionOperationNodeTest, ProgressNotation) {
  EXPECT_EQ(BuildOperationNode({3.f, 0.f, 1.f}, CalculationOperator::kProgress)
                ->Evaluate(FLT_MAX, {}),
            1.f);
  EXPECT_EQ(
      BuildOperationNode({10.f, 5.f, 10.f}, CalculationOperator::kProgress)
          ->Evaluate(FLT_MAX, {}),
      1.f);
  EXPECT_TRUE(std::isnan(
      BuildOperationNode({0.f, 0.f, 0.f}, CalculationOperator::kProgress)
          ->Evaluate(FLT_MAX, {})));
}

TEST(CalculationExpressionOperationNodeTest, ColorChannelKeywordNode) {
  const CalculationExpressionColorChannelKeywordNode* node =
      MakeGarbageCollected<CalculationExpressionColorChannelKeywordNode>(
          ColorChannelKeyword::kAlpha);
  EXPECT_TRUE(node->IsColorChannelKeyword());
  EXPECT_EQ(node->Value(), ColorChannelKeyword::kAlpha);
  EXPECT_EQ(node->Zoom(1), node);
}

TEST(CalculationExpressionOperationNodeTest, ColorChannelKeywordNode_Equals) {
  const CalculationExpressionColorChannelKeywordNode* node_1 =
      MakeGarbageCollected<CalculationExpressionColorChannelKeywordNode>(
          ColorChannelKeyword::kS);
  const CalculationExpressionColorChannelKeywordNode* node_1a =
      MakeGarbageCollected<CalculationExpressionColorChannelKeywordNode>(
          ColorChannelKeyword::kS);
  const CalculationExpressionColorChannelKeywordNode* node_2 =
      MakeGarbageCollected<CalculationExpressionColorChannelKeywordNode>(
          ColorChannelKeyword::kL);
  const CalculationExpressionNode* node_3 =
      MakeGarbageCollected<CalculationExpressionNumberNode>(-0.5f);

  EXPECT_TRUE(node_1->Equals(*node_1a));
  EXPECT_FALSE(node_1->Equals(*node_2));
  EXPECT_FALSE(node_1->Equals(*node_3));
}

TEST(CalculationExpressionOperationNodeTest, ColorChannelKeywordNode_Evaluate) {
  const CalculationExpressionNode* node_1 =
      MakeGarbageCollected<CalculationExpressionColorChannelKeywordNode>(
          ColorChannelKeyword::kH);
  const CalculationExpressionNode* node_2 =
      MakeGarbageCollected<CalculationExpressionNumberNode>(0.5f);

  const auto* operation_node =
      CalculationExpressionOperationNode::CreateSimplified(
          {node_1, node_2}, CalculationOperator::kMultiply);
  EvaluationInput evaluation_input;
  evaluation_input.color_channel_keyword_values = {
      {ColorChannelKeyword::kH, 120},
      {ColorChannelKeyword::kS, 0.3},
      {ColorChannelKeyword::kL, 0.6}};
  EXPECT_EQ(operation_node->Evaluate(FLT_MAX, evaluation_input), 60.f);

  // Test behavior when channel values are missing.
  EXPECT_EQ(operation_node->Evaluate(FLT_MAX, {}), 0.f);
}

}  // namespace blink
