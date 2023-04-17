// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(CalculationExpressionOperationNodeTest, Comparison) {
  scoped_refptr<CalculationExpressionNumberNode> operand1_1 =
      base::MakeRefCounted<CalculationExpressionNumberNode>(17.f);
  scoped_refptr<CalculationExpressionNumberNode> operand1_2 =
      base::MakeRefCounted<CalculationExpressionNumberNode>(13.f);

  scoped_refptr<CalculationExpressionNumberNode> operand2_1 =
      base::MakeRefCounted<CalculationExpressionNumberNode>(17.f);
  scoped_refptr<CalculationExpressionNumberNode> operand2_2 =
      base::MakeRefCounted<CalculationExpressionNumberNode>(13.f);

  CalculationExpressionOperationNode::Children operands1;
  operands1.push_back(operand1_1);
  operands1.push_back(operand1_2);

  CalculationExpressionOperationNode::Children operands2;
  operands2.push_back(operand2_1);
  operands2.push_back(operand2_2);

  scoped_refptr<CalculationExpressionOperationNode> operation1 =
      base::MakeRefCounted<CalculationExpressionOperationNode>(
          std::move(operands1), CalculationOperator::kMax);
  scoped_refptr<CalculationExpressionOperationNode> operation2 =
      base::MakeRefCounted<CalculationExpressionOperationNode>(
          std::move(operands2), CalculationOperator::kMax);

  EXPECT_EQ(*operation1, *operation2);
}

TEST(CalculationExpressionOperationNodeTest, SteppedValueFunctions) {
  scoped_refptr<CalculationExpressionNumberNode> operand_1 =
      base::MakeRefCounted<CalculationExpressionNumberNode>(1.f);
  scoped_refptr<CalculationExpressionNumberNode> operand_18 =
      base::MakeRefCounted<CalculationExpressionNumberNode>(18.f);
  scoped_refptr<CalculationExpressionNumberNode> operand_17 =
      base::MakeRefCounted<CalculationExpressionNumberNode>(17.f);
  scoped_refptr<CalculationExpressionNumberNode> operand_5 =
      base::MakeRefCounted<CalculationExpressionNumberNode>(5.f);

  CalculationExpressionOperationNode::Children operands_nearest_1_1;
  operands_nearest_1_1.push_back(operand_1);
  operands_nearest_1_1.push_back(operand_1);
  CalculationExpressionOperationNode::Children operands_mod_1_1(
      operands_nearest_1_1);
  CalculationExpressionOperationNode::Children operands_rem_1_1(
      operands_nearest_1_1);
  CalculationExpressionOperationNode::Children operands_mod_18_5;
  operands_mod_18_5.push_back(operand_18);
  operands_mod_18_5.push_back(operand_5);
  CalculationExpressionOperationNode::Children operands_mod_17_5;
  operands_mod_17_5.push_back(operand_17);
  operands_mod_17_5.push_back(operand_5);

  scoped_refptr<CalculationExpressionOperationNode> operation_nearest_1_1 =
      base::MakeRefCounted<CalculationExpressionOperationNode>(
          std::move(operands_nearest_1_1), CalculationOperator::kRoundNearest);
  scoped_refptr<CalculationExpressionOperationNode> operation_mod_1_1 =
      base::MakeRefCounted<CalculationExpressionOperationNode>(
          std::move(operands_mod_1_1), CalculationOperator::kMod);
  scoped_refptr<CalculationExpressionOperationNode> operation_rem_1_1 =
      base::MakeRefCounted<CalculationExpressionOperationNode>(
          std::move(operands_rem_1_1), CalculationOperator::kRem);
  scoped_refptr<CalculationExpressionOperationNode> operation_mod_18_5 =
      base::MakeRefCounted<CalculationExpressionOperationNode>(
          std::move(operands_mod_18_5), CalculationOperator::kMod);
  scoped_refptr<CalculationExpressionOperationNode> operation_mod_17_5 =
      base::MakeRefCounted<CalculationExpressionOperationNode>(
          std::move(operands_mod_17_5), CalculationOperator::kMod);

  CalculationExpressionOperationNode::Children operands_rem_two_mods;
  operands_rem_two_mods.push_back(operation_mod_18_5);
  operands_rem_two_mods.push_back(operation_mod_17_5);
  scoped_refptr<CalculationExpressionOperationNode> operation_rem_two_mods =
      base::MakeRefCounted<CalculationExpressionOperationNode>(
          std::move(operands_rem_two_mods), CalculationOperator::kRem);

  EXPECT_EQ(operation_nearest_1_1->Evaluate(FLT_MAX, nullptr), 1.f);
  EXPECT_EQ(operation_mod_1_1->Evaluate(FLT_MAX, nullptr), 0.f);
  EXPECT_EQ(operation_rem_1_1->Evaluate(FLT_MAX, nullptr), 0.f);
  EXPECT_EQ(operation_rem_two_mods->Evaluate(FLT_MAX, nullptr), 1.f);
}

}  // namespace

}  // namespace blink
