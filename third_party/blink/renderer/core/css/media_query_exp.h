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

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/conditional_exp_node.h"
#include "third_party/blink/renderer/core/css/css_length_resolver.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_ratio_value.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/media_feature_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/layout/geometry/axis.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class CSSParserContext;
class CSSParserTokenStream;

class CORE_EXPORT MediaQueryExpValue {
  DISALLOW_NEW();

 public:
  // Type::kInvalid
  MediaQueryExpValue() = default;

  explicit MediaQueryExpValue(CSSValueID id) : type_(Type::kId), id_(id) {}
  explicit MediaQueryExpValue(const CSSValue& value)
      : type_(Type::kValue), value_(value) {}
  MediaQueryExpValue(const CSSPrimitiveValue& numerator,
                     const CSSPrimitiveValue& denominator)
      : type_(Type::kRatio),
        ratio_(MakeGarbageCollected<cssvalue::CSSRatioValue>(numerator,
                                                             denominator)) {}
  void Trace(Visitor* visitor) const {
    visitor->Trace(value_);
    visitor->Trace(ratio_);
  }

  bool IsValid() const { return type_ != Type::kInvalid; }
  bool IsId() const { return type_ == Type::kId; }
  bool IsRatio() const { return type_ == Type::kRatio; }
  bool IsValue() const { return type_ == Type::kValue; }

  bool IsPrimitiveValue() const {
    return IsValue() && value_->IsPrimitiveValue();
  }
  bool IsNumber() const {
    return IsPrimitiveValue() && To<CSSPrimitiveValue>(*value_).IsNumber();
  }
  bool IsResolution() const {
    return IsPrimitiveValue() && To<CSSPrimitiveValue>(*value_).IsResolution();
  }
  bool IsNumericLiteralValue() const {
    return IsValue() && value_->IsNumericLiteralValue();
  }
  bool IsDotsPerCentimeter() const {
    return IsNumericLiteralValue() &&
           To<CSSNumericLiteralValue>(*value_).GetType() ==
               CSSPrimitiveValue::UnitType::kDotsPerCentimeter;
  }

  CSSValueID Id() const {
    DCHECK(IsId());
    return id_;
  }

  double GetDoubleValue() const {
    DCHECK(IsNumericLiteralValue());
    return To<CSSNumericLiteralValue>(*value_).ClampedDoubleValue();
  }

  CSSPrimitiveValue::UnitType GetUnitType() const {
    DCHECK(IsNumericLiteralValue());
    return To<CSSNumericLiteralValue>(*value_).GetType();
  }

  const CSSValue& GetCSSValue() const {
    DCHECK(IsValue());
    return *value_;
  }

  const CSSValue& Numerator() const {
    DCHECK(IsRatio());
    return ratio_->First();
  }

  const CSSValue& Denominator() const {
    DCHECK(IsRatio());
    return ratio_->Second();
  }

  double Value(const CSSLengthResolver& length_resolver) const {
    DCHECK(IsValue());
    return To<CSSPrimitiveValue>(*value_).ComputeValueInCanonicalUnit(
        length_resolver);
  }

  double Numerator(const CSSLengthResolver& length_resolver) const {
    DCHECK(IsRatio());
    return ratio_->First().ComputeValueInCanonicalUnit(length_resolver);
  }

  double Denominator(const CSSLengthResolver& length_resolver) const {
    DCHECK(IsRatio());
    return ratio_->Second().ComputeValueInCanonicalUnit(length_resolver);
  }

  enum UnitFlags {
    kNone = 0,
    kFontRelative = 1 << 0,
    kRootFontRelative = 1 << 1,
    kDynamicViewport = 1 << 2,
    kStaticViewport = 1 << 3,
    kContainer = 1 << 4,
    kTreeCounting = 1 << 5,
  };

  static const int kUnitFlagsBits = 6;

  unsigned GetUnitFlags() const;

  String CssText() const;
  bool operator==(const MediaQueryExpValue& other) const {
    if (type_ != other.type_) {
      return false;
    }
    switch (type_) {
      case Type::kInvalid:
        return true;
      case Type::kId:
        return id_ == other.id_;
      case Type::kValue:
        return base::ValuesEquivalent(value_, other.value_);
      case Type::kRatio:
        return base::ValuesEquivalent(ratio_, other.ratio_);
    }
  }

  // Consume a MediaQueryExpValue for the provided feature, which must already
  // be lower-cased.
  //
  // std::nullopt is returned on errors.
  static std::optional<MediaQueryExpValue> Consume(
      const String& lower_media_feature,
      CSSParserTokenStream&,
      const CSSParserContext&,
      bool supports_element_dependent);

 private:
  enum class Type { kInvalid, kId, kValue, kRatio };

  Type type_ = Type::kInvalid;

  CSSValueID id_;
  Member<const CSSValue> value_;
  Member<const cssvalue::CSSRatioValue> ratio_;
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
  void Trace(Visitor* visitor) const { visitor->Trace(value); }

  bool operator==(const MediaQueryExpComparison& o) const {
    return value == o.value && op == o.op;
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
  void Trace(Visitor* visitor) const {
    visitor->Trace(left);
    visitor->Trace(right);
  }

  bool IsRange() const {
    return left.op != MediaQueryOperator::kNone ||
           right.op != MediaQueryOperator::kNone;
  }

  bool operator==(const MediaQueryExpBounds& o) const {
    return left == o.left && right == o.right;
  }

  MediaQueryExpComparison left;
  MediaQueryExpComparison right;
};

class CORE_EXPORT MediaQueryExp {
  DISALLOW_NEW();

 public:
  // Returns an invalid MediaQueryExp if the arguments are invalid.
  static MediaQueryExp Create(const AtomicString& media_feature,
                              CSSParserTokenStream&,
                              const CSSParserContext&,
                              bool supports_element_dependent);
  static MediaQueryExp Create(const AtomicString& media_feature,
                              const MediaQueryExpBounds&);
  static MediaQueryExp Create(const AtomicString& custom_media);
  static MediaQueryExp Create(const MediaQueryExpValue& reference_value,
                              const MediaQueryExpBounds&);
  static MediaQueryExp Invalid() { return MediaQueryExp(); }

  MediaQueryExp(const MediaQueryExp& other);
  ~MediaQueryExp();
  void Trace(Visitor*) const;

  bool IsValid() const { return type_ != Type::kInvalid; }
  bool HasMediaFeature() const { return type_ == Type::kMediaFeature; }
  bool HasStyleRange() const { return type_ == Type::kStyleRange; }
  bool IsCustomMedia() const { return type_ == Type::kCustomMedia; }

  const AtomicString& MediaFeature() const {
    DCHECK(HasMediaFeature() || IsCustomMedia());
    return media_feature_;
  }

  const CSSUnparsedDeclarationValue& ReferenceValue() const {
    DCHECK(HasStyleRange());
    return *reference_value_;
  }

  const MediaQueryExpBounds& Bounds() const { return bounds_; }

  bool operator==(const MediaQueryExp& other) const;

  bool IsViewportDependent() const;

  bool IsDeviceDependent() const;

  bool IsWidthDependent() const;
  bool IsHeightDependent() const;
  bool IsInlineSizeDependent() const;
  bool IsBlockSizeDependent() const;

  String Serialize() const;

  // Return the union of GetUnitFlags() from the expr values.
  unsigned GetUnitFlags() const;

 private:
  enum class Type { kMediaFeature, kCustomMedia, kStyleRange, kInvalid };

  MediaQueryExp() = default;
  MediaQueryExp(const String& media_feature, const MediaQueryExpValue&);
  MediaQueryExp(const String& media_feature,
                const MediaQueryExpBounds&,
                Type type);
  MediaQueryExp(const CSSUnparsedDeclarationValue& reference_value,
                const MediaQueryExpBounds&);

  Type type_ = Type::kInvalid;
  // The `bounds_` member represents the values that `media_feature_` is
  // compared to, either of the left side, or right side, or both (see
  // `MediaQueryExpBounds`). If `reference_value_` is set, the `bounds_` are
  // compared to that instead.
  AtomicString media_feature_;
  Member<const CSSUnparsedDeclarationValue> reference_value_;
  MediaQueryExpBounds bounds_;
};

class CORE_EXPORT MediaQueryFeatureExpNode : public ConditionalExpNode {
 public:
  explicit MediaQueryFeatureExpNode(const MediaQueryExp& exp) : exp_(exp) {}
  void Trace(Visitor*) const override;

  const String& Name() const {
    DCHECK(HasMediaFeature() || IsCustomMedia());
    return exp_.MediaFeature();
  }

  const CSSUnparsedDeclarationValue& ReferenceValue() const {
    DCHECK(HasStyleRange());
    return exp_.ReferenceValue();
  }

  bool HasMediaFeature() const { return exp_.HasMediaFeature(); }
  bool HasStyleRange() const { return exp_.HasStyleRange(); }
  bool IsCustomMedia() const { return exp_.IsCustomMedia(); }

  const MediaQueryExpBounds& Bounds() const { return exp_.Bounds(); }

  unsigned GetUnitFlags() const;
  bool IsViewportDependent() const;
  bool IsDeviceDependent() const;
  bool IsWidthDependent() const;
  bool IsHeightDependent() const;
  bool IsInlineSizeDependent() const;
  bool IsBlockSizeDependent() const;

  KleeneValue Evaluate(ConditionalExpNodeVisitor&) const override;
  void SerializeTo(StringBuilder&) const override;

  const MediaQueryExp& GetMediaQueryExp() const { return exp_; }

 private:
  MediaQueryExp exp_;
};

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::MediaQueryExpValue)
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::MediaQueryExpComparison)
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::MediaQueryExpBounds)
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::MediaQueryExp)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_QUERY_EXP_H_
