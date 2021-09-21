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

class CORE_EXPORT MediaQueryExp {
  DISALLOW_NEW();

 public:
  // Returns an invalid MediaQueryExp if the arguments are invalid.
  static MediaQueryExp Create(const String& media_feature,
                              CSSParserTokenRange&,
                              const CSSParserContext&,
                              const ExecutionContext*);
  static MediaQueryExp Invalid() {
    return MediaQueryExp(String(), MediaQueryExpValue());
  }

  MediaQueryExp(const MediaQueryExp& other);
  ~MediaQueryExp();

  const String& MediaFeature() const { return media_feature_; }

  MediaQueryExpValue ExpValue() const { return exp_value_; }

  bool IsValid() const { return !media_feature_.IsNull(); }

  bool operator==(const MediaQueryExp& other) const;

  bool IsViewportDependent() const;

  bool IsDeviceDependent() const;

  bool IsWidthDependent() const;

  bool IsHeightDependent() const;

  String Serialize() const;

 private:
  MediaQueryExp(const String&, const MediaQueryExpValue&);

  String media_feature_;
  MediaQueryExpValue exp_value_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_QUERY_EXP_H_
