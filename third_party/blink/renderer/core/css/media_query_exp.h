/*
 * CSS Media Query
 *
 * Copyright (C) 2006 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_QUERY_EXP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_QUERY_EXP_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/media_feature_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSParserContext;
class CSSParserTokenRange;
class ExecutionContext;

class CORE_EXPORT MediaQueryExpValue {
  DISALLOW_NEW();

 public:
  // Type::kInvalid
  MediaQueryExpValue() = default;

  explicit MediaQueryExpValue(CSSValueID id) : type_(Type::kId), id_(id) {}
  MediaQueryExpValue(double value, CSSPrimitiveValue::UnitType unit)
      : type_(Type::kNumeric), numeric_({value, unit}) {}
  MediaQueryExpValue(unsigned numerator, unsigned denominator)
      : type_(Type::kRatio), ratio_({numerator, denominator}) {}

  bool IsValid() const { return type_ != Type::kInvalid; }
  bool IsId() const { return type_ == Type::kId; }
  bool IsNumeric() const { return type_ == Type::kNumeric; }
  bool IsRatio() const { return type_ == Type::kRatio; }

  CSSValueID Id() const {
    DCHECK(IsId());
    return id_;
  }

  double Value() const {
    DCHECK(IsNumeric());
    return numeric_.value;
  }

  CSSPrimitiveValue::UnitType Unit() const {
    DCHECK(IsNumeric());
    return numeric_.unit;
  }

  unsigned Numerator() const {
    DCHECK(IsRatio());
    return ratio_.numerator;
  }

  unsigned Denominator() const {
    DCHECK(IsRatio());
    return ratio_.denominator;
  }

  String CssText() const;
  bool operator==(const MediaQueryExpValue& other) const {
    if (type_ != other.type_)
      return false;
    switch (type_) {
      case Type::kInvalid:
        return true;
      case Type::kId:
        return id_ == other.id_;
      case Type::kNumeric:
        return (numeric_.value == other.numeric_.value) &&
               (numeric_.unit == other.numeric_.unit);
      case Type::kRatio:
        return (ratio_.numerator == other.ratio_.numerator) &&
               (ratio_.denominator == other.ratio_.denominator);
    }
  }
  bool operator!=(const MediaQueryExpValue& other) const {
    return !(*this == other);
  }

 private:
  enum class Type { kInvalid, kId, kNumeric, kRatio };

  Type type_ = Type::kInvalid;

  union {
    CSSValueID id_;
    struct {
      double value;
      CSSPrimitiveValue::UnitType unit;
    } numeric_;
    struct {
      unsigned numerator;
      unsigned denominator;
    } ratio_;
  };
};

// https://drafts.csswg.org/mediaqueries-4/#mq-syntax
enum class MediaQueryOperator {
  // Used for <mf-plain>, <mf-boolean>
  kNone,

  // Used for <mf-range>
  kEq,
  kLt,
  kLe,
  kGt,
  kGe,
};

// This represents the following part of a <media-feature> (example):
//
//  (width >= 10px)
//         ^^^^^^^
//
struct CORE_EXPORT MediaQueryExpComparison {
  DISALLOW_NEW();
  MediaQueryExpComparison() = default;
  explicit MediaQueryExpComparison(const MediaQueryExpValue& value)
      : value(value) {}
  MediaQueryExpComparison(const MediaQueryExpValue& value,
                          MediaQueryOperator op)
      : value(value), op(op) {}

  bool operator==(const MediaQueryExpComparison& o) const {
    return value == o.value && op == o.op;
  }
  bool operator!=(const MediaQueryExpComparison& o) const {
    return !(*this == o);
  }

  bool IsValid() const { return value.IsValid(); }

  MediaQueryExpValue value;
  MediaQueryOperator op = MediaQueryOperator::kNone;
};

// There exists three types of <media-feature>s.
//
//  1) Boolean features, which is just the feature name, e.g. (color)
//  2) Plain features, which can appear in two different forms:
//       - Feature with specific value, e.g. (width: 100px)
//       - Feature with min/max prefix, e.g. (min-width: 100px)
//  3) Range features, which can appear in three different forms:
//       - Feature compared with value, e.g. (width >= 100px)
//       - Feature compared with value (reversed), e.g. (100px <= width)
//       - Feature within a certain range, e.g. (100px < width < 200px)
//
// In the first case, both |left| and |right| values are not set.
// In the second case, only |right| is set.
// In the third case, either |left| is set, |right| is set, or both, depending
// on the form.
//
// https://drafts.csswg.org/mediaqueries-4/#typedef-media-feature
struct CORE_EXPORT MediaQueryExpBounds {
  DISALLOW_NEW();
  MediaQueryExpBounds() = default;
  explicit MediaQueryExpBounds(const MediaQueryExpComparison& right)
      : right(right) {}
  MediaQueryExpBounds(const MediaQueryExpComparison& left,
                      const MediaQueryExpComparison& right)
      : left(left), right(right) {}

  bool IsRange() const {
    return left.op != MediaQueryOperator::kNone ||
           right.op != MediaQueryOperator::kNone;
  }

  bool operator==(const MediaQueryExpBounds& o) const {
    return left == o.left && right == o.right;
  }
  bool operator!=(const MediaQueryExpBounds& o) const { return !(*this == o); }

  MediaQueryExpComparison left;
  MediaQueryExpComparison right;
};

class CORE_EXPORT MediaQueryExp {
  DISALLOW_NEW();

 public:
  // Returns an invalid MediaQueryExp if the arguments are invalid.
  static MediaQueryExp Create(const String& media_feature,
                              CSSParserTokenRange&,
                              const CSSParserContext&,
                              const ExecutionContext*);
  static MediaQueryExp Create(const String& media_feature,
                              const MediaQueryExpBounds&);
  static MediaQueryExp Invalid() {
    return MediaQueryExp(String(), MediaQueryExpValue());
  }

  MediaQueryExp(const MediaQueryExp& other);
  ~MediaQueryExp();

  const String& MediaFeature() const { return media_feature_; }

  // TODO(crbug.com/1034465): Replace with MediaQueryExpBounds.
  MediaQueryExpValue ExpValue() const {
    DCHECK(!bounds_.left.IsValid());
    return bounds_.right.value;
  }

  const MediaQueryExpBounds& Bounds() const { return bounds_; }

  bool IsValid() const { return !media_feature_.IsNull(); }

  bool operator==(const MediaQueryExp& other) const;
  bool operator!=(const MediaQueryExp& other) const {
    return !(*this == other);
  }

  bool IsViewportDependent() const;

  bool IsDeviceDependent() const;

  bool IsWidthDependent() const;

  bool IsHeightDependent() const;

  String Serialize() const;

 private:
  MediaQueryExp(const String&, const MediaQueryExpValue&);
  MediaQueryExp(const String&, const MediaQueryExpBounds&);

  String media_feature_;
  MediaQueryExpBounds bounds_;
};

// MediaQueryExpNode representing a tree of MediaQueryExp objects capable of
// nested/compound expressions.
class CORE_EXPORT MediaQueryExpNode {
  USING_FAST_MALLOC(MediaQueryExpNode);

 public:
  virtual ~MediaQueryExpNode() = default;

  enum class Type { kFeature, kNested, kNot, kAnd, kOr };

  String Serialize() const;

  virtual Type GetType() const = 0;
  virtual void SerializeTo(StringBuilder&) const = 0;
  virtual std::unique_ptr<MediaQueryExpNode> Copy() const = 0;
};

class CORE_EXPORT MediaQueryFeatureExpNode : public MediaQueryExpNode {
  USING_FAST_MALLOC(MediaQueryFeatureExpNode);

 public:
  explicit MediaQueryFeatureExpNode(const MediaQueryExp& exp) : exp_(exp) {}

  MediaQueryExp Expression() const { return exp_; }

  Type GetType() const override { return Type::kFeature; }
  void SerializeTo(StringBuilder&) const override;
  std::unique_ptr<MediaQueryExpNode> Copy() const override;

 private:
  MediaQueryExp exp_;
};

class CORE_EXPORT MediaQueryNestedExpNode : public MediaQueryExpNode {
  USING_FAST_MALLOC(MediaQueryNestedExpNode);

 public:
  explicit MediaQueryNestedExpNode(std::unique_ptr<MediaQueryExpNode> child)
      : child_(std::move(child)) {
    DCHECK(child_);
  }

  const MediaQueryExpNode& Child() const { return *child_; }

  Type GetType() const override { return Type::kNested; }
  void SerializeTo(StringBuilder&) const override;
  std::unique_ptr<MediaQueryExpNode> Copy() const override;

 private:
  std::unique_ptr<MediaQueryExpNode> child_;
};

class CORE_EXPORT MediaQueryNotExpNode : public MediaQueryExpNode {
  USING_FAST_MALLOC(MediaQueryNotExpNode);

 public:
  explicit MediaQueryNotExpNode(std::unique_ptr<MediaQueryExpNode> operand)
      : operand_(std::move(operand)) {
    DCHECK(operand_);
  }

  const MediaQueryExpNode& Operand() const { return *operand_; }

  Type GetType() const override { return Type::kNot; }
  void SerializeTo(StringBuilder&) const override;
  std::unique_ptr<MediaQueryExpNode> Copy() const override;

 private:
  std::unique_ptr<MediaQueryExpNode> operand_;
};

class CORE_EXPORT MediaQueryCompoundExpNode : public MediaQueryExpNode {
  USING_FAST_MALLOC(MediaQueryCompoundExpNode);

 public:
  MediaQueryCompoundExpNode(std::unique_ptr<MediaQueryExpNode> left,
                            std::unique_ptr<MediaQueryExpNode> right)
      : left_(std::move(left)), right_(std::move(right)) {
    DCHECK(left_);
    DCHECK(right_);
  }

  const MediaQueryExpNode& Left() const { return *left_; }
  const MediaQueryExpNode& Right() const { return *right_; }

 private:
  std::unique_ptr<MediaQueryExpNode> left_;
  std::unique_ptr<MediaQueryExpNode> right_;
};

class CORE_EXPORT MediaQueryAndExpNode : public MediaQueryCompoundExpNode {
  USING_FAST_MALLOC(MediaQueryAndExpNode);

 public:
  MediaQueryAndExpNode(std::unique_ptr<MediaQueryExpNode> left,
                       std::unique_ptr<MediaQueryExpNode> right)
      : MediaQueryCompoundExpNode(std::move(left), std::move(right)) {}

  Type GetType() const override { return Type::kAnd; }
  void SerializeTo(StringBuilder&) const override;
  std::unique_ptr<MediaQueryExpNode> Copy() const override;
};

class CORE_EXPORT MediaQueryOrExpNode : public MediaQueryCompoundExpNode {
  USING_FAST_MALLOC(MediaQueryOrExpNode);

 public:
  MediaQueryOrExpNode(std::unique_ptr<MediaQueryExpNode> left,
                      std::unique_ptr<MediaQueryExpNode> right)
      : MediaQueryCompoundExpNode(std::move(left), std::move(right)) {}

  Type GetType() const override { return Type::kOr; }
  void SerializeTo(StringBuilder&) const override;
  std::unique_ptr<MediaQueryExpNode> Copy() const override;
};

template <>
struct DowncastTraits<MediaQueryFeatureExpNode> {
  static bool AllowFrom(const MediaQueryExpNode& node) {
    return node.GetType() == MediaQueryExpNode::Type::kFeature;
  }
};

template <>
struct DowncastTraits<MediaQueryNestedExpNode> {
  static bool AllowFrom(const MediaQueryExpNode& node) {
    return node.GetType() == MediaQueryExpNode::Type::kNested;
  }
};

template <>
struct DowncastTraits<MediaQueryNotExpNode> {
  static bool AllowFrom(const MediaQueryExpNode& node) {
    return node.GetType() == MediaQueryExpNode::Type::kNot;
  }
};

template <>
struct DowncastTraits<MediaQueryAndExpNode> {
  static bool AllowFrom(const MediaQueryExpNode& node) {
    return node.GetType() == MediaQueryExpNode::Type::kAnd;
  }
};

template <>
struct DowncastTraits<MediaQueryOrExpNode> {
  static bool AllowFrom(const MediaQueryExpNode& node) {
    return node.GetType() == MediaQueryExpNode::Type::kOr;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_QUERY_EXP_H_
