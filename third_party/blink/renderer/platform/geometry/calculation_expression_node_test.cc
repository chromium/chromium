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

}  // namespace

}  // namespace blink
