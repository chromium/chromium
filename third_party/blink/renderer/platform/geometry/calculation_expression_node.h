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

enum class CalculationOperator {
  kAdd,
  kSubtract,
  kMultiply,  // Division is converted to multiplication and use this value too.
  kMin,
  kMax,
  kClamp,
  kInvalid
};

// Represents an expression composed of numbers, |PixelsAndPercent| and multiple
// types of operators. To be consumed by |Length| values that involve
// non-trivial math functions like min() and max().
class PLATFORM_EXPORT CalculationExpressionNode
    : public RefCounted<CalculationExpressionNode> {
 public:
  virtual float Evaluate(float max_value) const = 0;
  virtual bool operator==(const CalculationExpressionNode& other) const = 0;
  bool operator!=(const CalculationExpressionNode& other) const {
    return !operator==(other);
  }

  virtual bool IsNumber() const { return false; }
  virtual bool IsPixelsAndPercent() const { return false; }
  virtual bool IsOperation() const { return false; }

  virtual scoped_refptr<const CalculationExpressionNode> Zoom(
      double factor) const = 0;

  virtual ~CalculationExpressionNode() = default;

#if DCHECK_IS_ON()
  enum class ResultType { kInvalid, kNumber, kPixelsAndPercent };

  virtual ResultType ResolvedResultType() const = 0;

 protected:
  ResultType result_type_;
#endif
};

class PLATFORM_EXPORT CalculationExpressionNumberNode final
    : public CalculationExpressionNode {
 public:
  CalculationExpressionNumberNode(float value) : value_(value) {
#if DCHECK_IS_ON()
    result_type_ = ResultType::kNumber;
#endif
  }

  float Value() const { return value_; }

  // Implement |CalculationExpressionNode|:
  float Evaluate(float max_value) const final;
  bool operator==(const CalculationExpressionNode& other) const final;
  scoped_refptr<const CalculationExpressionNode> Zoom(
      double factor) const final;
  bool IsNumber() const final { return true; }
  ~CalculationExpressionNumberNode() final = default;

#if DCHECK_IS_ON()
  ResultType ResolvedResultType() const final;
#endif

 private:
  float value_;
};

template <>
struct DowncastTraits<CalculationExpressionNumberNode> {
  static bool AllowFrom(const CalculationExpressionNode& node) {
    return node.IsNumber();
  }
};

class PLATFORM_EXPORT CalculationExpressionPixelsAndPercentNode final
    : public CalculationExpressionNode {
 public:
  CalculationExpressionPixelsAndPercentNode(PixelsAndPercent value)
      : value_(value) {
#if DCHECK_IS_ON()
    result_type_ = ResultType::kPixelsAndPercent;
#endif
  }

  float Pixels() const { return value_.pixels; }
  float Percent() const { return value_.percent; }
  PixelsAndPercent GetPixelsAndPercent() const { return value_; }

  // Implement |CalculationExpressionNode|:
  float Evaluate(float max_value) const final;
  bool operator==(const CalculationExpressionNode& other) const final;
  scoped_refptr<const CalculationExpressionNode> Zoom(
      double factor) const final;
  bool IsPixelsAndPercent() const final { return true; }
  ~CalculationExpressionPixelsAndPercentNode() final = default;

#if DCHECK_IS_ON()
  ResultType ResolvedResultType() const final;
#endif

 private:
  PixelsAndPercent value_;
};

template <>
struct DowncastTraits<CalculationExpressionPixelsAndPercentNode> {
  static bool AllowFrom(const CalculationExpressionNode& node) {
    return node.IsPixelsAndPercent();
  }
};

class PLATFORM_EXPORT CalculationExpressionOperationNode final
    : public CalculationExpressionNode {
 public:
  using Children = Vector<scoped_refptr<const CalculationExpressionNode>>;

  static scoped_refptr<const CalculationExpressionNode> CreateSimplified(
      Children&& children,
      CalculationOperator op);

  CalculationExpressionOperationNode(Children&& children, CalculationOperator op)
      : children_(std::move(children)), operator_(op) {
#if DCHECK_IS_ON()
    result_type_ = ResolvedResultType();
    DCHECK_NE(result_type_, ResultType::kInvalid);
#endif
  }

  const Children& GetChildren() const { return children_; }
  CalculationOperator GetOperator() const { return operator_; }

  // Implement |CalculationExpressionNode|:
  float Evaluate(float max_value) const final;
  bool operator==(const CalculationExpressionNode& other) const final;
  scoped_refptr<const CalculationExpressionNode> Zoom(
      double factor) const final;
  bool IsOperation() const final { return true; }
  ~CalculationExpressionOperationNode() final = default;

#if DCHECK_IS_ON()
  ResultType ResolvedResultType() const final;
#endif

 private:
  Children children_;
  CalculationOperator operator_;
};

template <>
struct DowncastTraits<CalculationExpressionOperationNode> {
  static bool AllowFrom(const CalculationExpressionNode& node) {
    return node.IsOperation();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_CALCULATION_EXPRESSION_NODE_H_
