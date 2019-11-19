// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_CALCULATION_EXPRESSION_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_CALCULATION_EXPRESSION_NODE_H_

#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Represents an expression composed of |PixelsAndPercent| and multiple types of
// operators. To be consumed by |Length| values that involve non-trivial math
// functions like min() and max().
class PLATFORM_EXPORT CalculationExpressionNode
    : public RefCounted<CalculationExpressionNode> {
 public:
  virtual float Evaluate(float max_value) const = 0;
  virtual bool operator==(const CalculationExpressionNode& other) const = 0;
  bool operator!=(const CalculationExpressionNode& other) const {
    return !operator==(other);
  }

  virtual bool IsLeaf() const { return false; }
  virtual bool IsMultiplication() const { return false; }
  virtual bool IsAdditive() const { return false; }
  virtual bool IsComparison() const { return false; }

  virtual scoped_refptr<const CalculationExpressionNode> Zoom(
      double factor) const = 0;

  virtual ~CalculationExpressionNode() = default;
};

class PLATFORM_EXPORT CalculationExpressionLeafNode final
    : public CalculationExpressionNode {
 public:
  CalculationExpressionLeafNode(PixelsAndPercent value) : value_(value) {}

  float Pixels() const { return value_.pixels; }
  float Percent() const { return value_.percent; }
  PixelsAndPercent GetPixelsAndPercent() const { return value_; }

  // Implement |CalculationExpressionNode|:
  float Evaluate(float max_value) const final;
  bool operator==(const CalculationExpressionNode& other) const final;
  scoped_refptr<const CalculationExpressionNode> Zoom(
      double factor) const final;
  bool IsLeaf() const final { return true; }
  ~CalculationExpressionLeafNode() final = default;

 private:
  PixelsAndPercent value_;
};

template <>
struct DowncastTraits<CalculationExpressionLeafNode> {
  static bool AllowFrom(const CalculationExpressionNode& node) {
    return node.IsLeaf();
  }
};

class PLATFORM_EXPORT CalculationExpressionMultiplicationNode final
    : public CalculationExpressionNode {
 public:
  static scoped_refptr<const CalculationExpressionNode> CreateSimplified(
      scoped_refptr<const CalculationExpressionNode> node,
      float factor);

  CalculationExpressionMultiplicationNode(
      scoped_refptr<const CalculationExpressionNode> node,
      float factor)
      : child_(std::move(node)), factor_(factor) {}

  const CalculationExpressionNode& GetChild() const { return *child_; }
  float GetFactor() const { return factor_; }

  // Implement |CalculationExpressionNode|:
  float Evaluate(float max_value) const final;
  bool operator==(const CalculationExpressionNode& other) const final;
  scoped_refptr<const CalculationExpressionNode> Zoom(
      double factor) const final;
  bool IsMultiplication() const final { return true; }
  ~CalculationExpressionMultiplicationNode() final = default;

 private:
  scoped_refptr<const CalculationExpressionNode> child_;
  float factor_;
};

template <>
struct DowncastTraits<CalculationExpressionMultiplicationNode> {
  static bool AllowFrom(const CalculationExpressionNode& node) {
    return node.IsMultiplication();
  }
};

class PLATFORM_EXPORT CalculationExpressionAdditiveNode final
    : public CalculationExpressionNode {
 public:
  enum class Type { kAdd, kSubtract };

  static scoped_refptr<const CalculationExpressionNode> CreateSimplified(
      scoped_refptr<const CalculationExpressionNode> lhs,
      scoped_refptr<const CalculationExpressionNode> rhs,
      Type type);

  CalculationExpressionAdditiveNode(
      scoped_refptr<const CalculationExpressionNode> lhs,
      scoped_refptr<const CalculationExpressionNode> rhs,
      Type type)
      : lhs_(std::move(lhs)), rhs_(std::move(rhs)), type_(type) {}

  const CalculationExpressionNode& GetLeftSide() const { return *lhs_; }
  const CalculationExpressionNode& GetRightSide() const { return *rhs_; }
  bool IsAdd() const { return type_ == Type::kAdd; }
  bool IsSubtract() const { return type_ == Type::kSubtract; }

  // Implement |CalculationExpressionNode|:
  float Evaluate(float max_value) const final;
  bool operator==(const CalculationExpressionNode& other) const final;
  scoped_refptr<const CalculationExpressionNode> Zoom(
      double factor) const final;
  bool IsAdditive() const final { return true; }
  ~CalculationExpressionAdditiveNode() final = default;

 private:
  scoped_refptr<const CalculationExpressionNode> lhs_;
  scoped_refptr<const CalculationExpressionNode> rhs_;
  Type type_;
};

template <>
struct DowncastTraits<CalculationExpressionAdditiveNode> {
  static bool AllowFrom(const CalculationExpressionNode& node) {
    return node.IsAdditive();
  }
};

class PLATFORM_EXPORT CalculationExpressionComparisonNode final
    : public CalculationExpressionNode {
 public:
  enum class Type { kMin, kMax };

  static scoped_refptr<const CalculationExpressionNode> CreateSimplified(
      Vector<scoped_refptr<const CalculationExpressionNode>>&& operands,
      Type type);

  CalculationExpressionComparisonNode(
      Vector<scoped_refptr<const CalculationExpressionNode>>&& operands,
      Type type)
      : operands_(std::move(operands)), type_(type) {}

  const Vector<scoped_refptr<const CalculationExpressionNode>>& GetOperands()
      const {
    return operands_;
  }

  bool IsMin() const { return type_ == Type::kMin; }
  bool IsMax() const { return type_ == Type::kMax; }

  // Implement |CalculationExpressionNode|:
  float Evaluate(float max_value) const final;
  bool operator==(const CalculationExpressionNode& other) const final;
  scoped_refptr<const CalculationExpressionNode> Zoom(
      double factor) const final;
  bool IsComparison() const final { return true; }
  ~CalculationExpressionComparisonNode() final = default;

 private:
  Vector<scoped_refptr<const CalculationExpressionNode>> operands_;
  Type type_;
};

template <>
struct DowncastTraits<CalculationExpressionComparisonNode> {
  static bool AllowFrom(const CalculationExpressionNode& node) {
    return node.IsComparison();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_CALCULATION_EXPRESSION_NODE_H_
