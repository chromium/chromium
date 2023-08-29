/*
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
    Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
    Copyright (C) 2011 Rik Cabanier (cabanier@adobe.com)
    Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LENGTH_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LENGTH_H_

#include <cmath>
#include <cstring>

#include "base/check_op.h"
#include "base/notreached.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

struct PixelsAndPercent {
  DISALLOW_NEW();
  PixelsAndPercent(float pixels, float percent)
      : pixels(pixels), percent(percent) {}
  float pixels;
  float percent;
};

class CalculationExpressionNode;
class CalculationValue;
class Length;

PLATFORM_EXPORT extern const Length& g_auto_length;
PLATFORM_EXPORT extern const Length& g_none_length;

class PLATFORM_EXPORT Length {
  DISALLOW_NEW();

 public:
  // Initializes global instances.
  static void Initialize();

  enum class ValueRange { kAll, kNonNegative };

  // FIXME: This enum makes it hard to tell in general what values may be
  // appropriate for any given Length.
  enum Type : unsigned char {
    kAuto,
    kPercent,
    kFixed,
    kMinContent,
    kMaxContent,
    kMinIntrinsic,
    kFillAvailable,
    kFitContent,
    kCalculated,
    kFlex,
    kExtendToZoom,
    kDeviceWidth,
    kDeviceHeight,
    kNone,    // only valid for max-width, max-height, or contain-intrinsic-size
    kContent  // only valid for flex-basis
  };

  Length() : value_(0), type_(kAuto) {}

  explicit Length(Length::Type t) : value_(0), type_(t) {
    DCHECK_NE(t, kCalculated);
  }

  Length(int v, Length::Type t) : value_(v), type_(t) {
    DCHECK_NE(t, kCalculated);
  }

  Length(LayoutUnit v, Length::Type t) : value_(v.ToFloat()), type_(t) {
    DCHECK(std::isfinite(v.ToFloat()));
    DCHECK_NE(t, kCalculated);
  }

  Length(float v, Length::Type t) : value_(v), type_(t) {
    DCHECK(std::isfinite(v));
    DCHECK_NE(t, kCalculated);
  }

  Length(double v, Length::Type t) : type_(t) {
    DCHECK(std::isfinite(v));
    value_ = ClampTo<float>(v);
  }

  explicit Length(scoped_refptr<const CalculationValue>);

  Length(const Length& length) {
    memcpy(this, &length, sizeof(Length));
    if (IsCalculated())
      IncrementCalculatedRef();
  }

  Length& operator=(const Length& length) {
    if (length.IsCalculated())
      length.IncrementCalculatedRef();
    if (IsCalculated())
      DecrementCalculatedRef();
    memcpy(this, &length, sizeof(Length));
    return *this;
  }

  ~Length() {
    if (IsCalculated())
      DecrementCalculatedRef();
  }

  bool operator==(const Length& o) const {
    if (type_ != o.type_ || quirk_ != o.quirk_) {
      return false;
    }
    if (type_ == kCalculated) {
      return IsCalculatedEqual(o);
    } else {
      // For everything that doesn't use value_, it is defined to be zero,
      // so we can compare here unconditionally.
      return value_ == o.value_;
    }
  }
  bool operator!=(const Length& o) const { return !(*this == o); }

  template <typename NUMBER_TYPE>
  static Length Fixed(NUMBER_TYPE number) {
    return Length(number, kFixed);
  }
  static Length Fixed() { return Length(kFixed); }
  static const Length& Auto() { return g_auto_length; }
  static Length FillAvailable() { return Length(kFillAvailable); }
  static Length MinContent() { return Length(kMinContent); }
  static Length MaxContent() { return Length(kMaxContent); }
  static Length MinIntrinsic() { return Length(kMinIntrinsic); }
  static Length ExtendToZoom() { return Length(kExtendToZoom); }
  static Length DeviceWidth() { return Length(kDeviceWidth); }
  static Length DeviceHeight() { return Length(kDeviceHeight); }
  static const Length& None() { return g_none_length; }
  static Length FitContent() { return Length(kFitContent); }
  static Length Content() { return Length(kContent); }
  template <typename NUMBER_TYPE>
  static Length Percent(NUMBER_TYPE number) {
    return Length(number, kPercent);
  }
  static Length Flex(double value) { return Length(value, kFlex); }

  // FIXME: Make this private (if possible) or at least rename it
  // (http://crbug.com/432707).
  inline float Value() const {
    DCHECK(!IsCalculated());
    return GetFloatValue();
  }

  int IntValue() const {
    if (IsCalculated()) {
      NOTREACHED();
      return 0;
    }
    DCHECK(!IsNone());
    return static_cast<int>(value_);
  }

  float Pixels() const {
    DCHECK_EQ(GetType(), kFixed);
    return GetFloatValue();
  }

  float Percent() const {
    DCHECK_EQ(GetType(), kPercent);
    return GetFloatValue();
  }

  PixelsAndPercent GetPixelsAndPercent() const;

  const CalculationValue& GetCalculationValue() const;

  // If |this| is calculated, returns the underlying |CalculationValue|. If not,
  // returns a |CalculationValue| constructed from |GetPixelsAndPercent()|. Hits
  // a DCHECK if |this| is not a specified value (e.g., 'auto').
  scoped_refptr<const CalculationValue> AsCalculationValue() const;

  Length::Type GetType() const { return static_cast<Length::Type>(type_); }
  bool Quirk() const { return quirk_; }

  void SetQuirk(bool quirk) { quirk_ = quirk; }

  bool IsNone() const { return GetType() == kNone; }

  // FIXME calc: https://bugs.webkit.org/show_bug.cgi?id=80357. A calculated
  // Length always contains a percentage, and without a maxValue passed to these
  // functions it's impossible to determine the sign or zero-ness. We assume all
  // calc values are positive and non-zero for now.
  bool IsZero() const {
    DCHECK(!IsNone());
    if (IsCalculated())
      return false;

    return !value_;
  }
  bool IsPositive() const {
    if (IsNone())
      return false;
    if (IsCalculated())
      return true;

    return GetFloatValue() > 0;
  }
  bool IsNegative() const {
    if (IsNone() || IsCalculated())
      return false;

    return GetFloatValue() < 0;
  }

  // For the layout purposes, if this |Length| is a block-axis size, see
  // |IsAutoOrContentOrIntrinsic()|, it is usually a better choice.
  bool IsAuto() const { return GetType() == kAuto; }
  bool IsFixed() const { return GetType() == kFixed; }

  // For the block axis, intrinsic sizes such as `min-content` behave the same
  // as `auto`. https://www.w3.org/TR/css-sizing-3/#valdef-width-min-content
  bool IsContentOrIntrinsic() const {
    return GetType() == kMinContent || GetType() == kMaxContent ||
           GetType() == kFitContent || GetType() == kMinIntrinsic ||
           GetType() == kContent;
  }
  bool IsAutoOrContentOrIntrinsic() const {
    return GetType() == kAuto || IsContentOrIntrinsic();
  }

  bool IsSpecified() const {
    return GetType() == kFixed || GetType() == kPercent ||
           GetType() == kCalculated;
  }

  bool IsCalculated() const { return GetType() == kCalculated; }
  bool IsCalculatedEqual(const Length&) const;
  bool IsMinContent() const { return GetType() == kMinContent; }
  bool IsMaxContent() const { return GetType() == kMaxContent; }
  bool IsContent() const { return GetType() == kContent; }
  bool IsMinIntrinsic() const { return GetType() == kMinIntrinsic; }
  bool IsFillAvailable() const { return GetType() == kFillAvailable; }
  bool IsFitContent() const { return GetType() == kFitContent; }
  bool IsPercent() const { return GetType() == kPercent; }
  bool IsPercentOrCalc() const {
    return GetType() == kPercent || GetType() == kCalculated;
  }
  bool IsPercentOrCalcOrStretch() const {
    return GetType() == kPercent || GetType() == kCalculated ||
           GetType() == kFillAvailable;
  }
  bool IsFlex() const { return GetType() == kFlex; }
  bool IsExtendToZoom() const { return GetType() == kExtendToZoom; }
  bool IsDeviceWidth() const { return GetType() == kDeviceWidth; }
  bool IsDeviceHeight() const { return GetType() == kDeviceHeight; }
  bool HasAnchorQueries() const;
  bool HasAutoAnchorPositioning() const;

  Length Blend(const Length& from, double progress, ValueRange range) const {
    DCHECK(IsSpecified());
    DCHECK(from.IsSpecified());

    if (progress == 0.0)
      return from;

    if (progress == 1.0)
      return *this;

    if (from.GetType() == kCalculated || GetType() == kCalculated)
      return BlendMixedTypes(from, progress, range);

    if (!from.IsZero() && !IsZero() && from.GetType() != GetType())
      return BlendMixedTypes(from, progress, range);

    if (from.IsZero() && IsZero())
      return *this;

    return BlendSameTypes(from, progress, range);
  }

  float GetFloatValue() const {
    DCHECK(!IsNone());
    DCHECK(!IsCalculated());
    return value_;
  }

  class PLATFORM_EXPORT AnchorEvaluator {
   public:
    // Evaluates an anchor() or anchor-size() function given by the
    // CalculationExpressionNode. Returns |nullopt| if the query is invalid
    // (e.g., no targets or wrong axis.)
    virtual absl::optional<LayoutUnit> Evaluate(
        const CalculationExpressionNode&) const = 0;
  };
  float NonNanCalculatedValue(float max_value,
                              const AnchorEvaluator* = nullptr) const;

  Length SubtractFromOneHundredPercent() const;

  Length Zoom(double factor) const;

  String ToString() const;

 private:
  Length BlendMixedTypes(const Length& from, double progress, ValueRange) const;

  Length BlendSameTypes(const Length& from, double progress, ValueRange) const;

  int CalculationHandle() const {
    DCHECK(IsCalculated());
    return calculation_handle_;
  }
  void IncrementCalculatedRef() const;
  void DecrementCalculatedRef() const;

  union {
    // If kType == kCalculated.
    int calculation_handle_;

    // Otherwise. Must be zero if not in use (e.g., for kAuto or kNone).
    float value_;
  };
  bool quirk_ = false;
  unsigned char type_;
};

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const Length&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LENGTH_H_
