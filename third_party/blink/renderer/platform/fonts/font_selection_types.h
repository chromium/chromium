/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_SELECTION_TYPES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_SELECTION_TYPES_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Unclamped, unchecked, signed fixed-point number representing a value used for
// font variations. Sixteen bits in total, one sign bit, two fractional bits,
// means the smallest positive representable value is 0.25, the maximum
// representable value is 8191.75, and the minimum representable value is -8192.
class PLATFORM_EXPORT FontSelectionValue {
  USING_FAST_MALLOC(FontSelectionValue);

 public:
  FontSelectionValue() = default;

  // Explicit because it is lossy.
  explicit FontSelectionValue(int x) : backing_(x * fractionalEntropy) {}

  // Explicit because it is lossy.
  explicit FontSelectionValue(float x) : backing_(x * fractionalEntropy) {}

  // Explicit because it is lossy.
  explicit FontSelectionValue(double x) : backing_(x * fractionalEntropy) {}

  operator float() const {
    // floats have 23 fractional bits, but only 14 fractional bits are
    // necessary, so every value can be represented losslessly.
    return backing_ / static_cast<float>(fractionalEntropy);
  }

  FontSelectionValue operator+(const FontSelectionValue other) const;
  FontSelectionValue operator-(const FontSelectionValue other) const;
  FontSelectionValue operator*(const FontSelectionValue other) const;
  FontSelectionValue operator/(const FontSelectionValue other) const;
  FontSelectionValue operator-() const;
  bool operator==(const FontSelectionValue other) const;
  bool operator!=(const FontSelectionValue other) const;
  bool operator<(const FontSelectionValue other) const;
  bool operator<=(const FontSelectionValue other) const;
  bool operator>(const FontSelectionValue other) const;
  bool operator>=(const FontSelectionValue other) const;

  int16_t RawValue() const { return backing_; }

  String ToString() const;

  static const FontSelectionValue& MaximumValue() {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        const FontSelectionValue, maximumValue,
        (std::numeric_limits<int16_t>::max(), RawTag::RawTag));
    return maximumValue;
  }

  static const FontSelectionValue& MinimumValue() {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        const FontSelectionValue, minimumValue,
        (std::numeric_limits<int16_t>::min(), RawTag::RawTag));
    return minimumValue;
  }

 protected:
  enum class RawTag { RawTag };

  FontSelectionValue(int16_t rawValue, RawTag) : backing_(rawValue) {}

 private:
  static constexpr int fractionalEntropy = 4;
  // TODO(drott) crbug.com/745910 - Consider making this backed by a checked
  // arithmetic type.
  int16_t backing_{0};
};

inline FontSelectionValue FontSelectionValue::operator+(
    const FontSelectionValue other) const {
  return FontSelectionValue(backing_ + other.backing_, RawTag::RawTag);
}

inline FontSelectionValue FontSelectionValue::operator-(
    const FontSelectionValue other) const {
  return FontSelectionValue(backing_ - other.backing_, RawTag::RawTag);
}

inline FontSelectionValue FontSelectionValue::operator*(
    const FontSelectionValue other) const {
  return FontSelectionValue(
      static_cast<int32_t>(backing_) * other.backing_ / fractionalEntropy,
      RawTag::RawTag);
}

inline FontSelectionValue FontSelectionValue::operator/(
    const FontSelectionValue other) const {
  return FontSelectionValue(
      static_cast<int32_t>(backing_) / other.backing_ * fractionalEntropy,
      RawTag::RawTag);
}

inline FontSelectionValue FontSelectionValue::operator-() const {
  return FontSelectionValue(-backing_, RawTag::RawTag);
}

inline bool FontSelectionValue::operator==(
    const FontSelectionValue other) const {
  return backing_ == other.backing_;
}

inline bool FontSelectionValue::operator!=(
    const FontSelectionValue other) const {
  return !operator==(other);
}

inline bool FontSelectionValue::operator<(
    const FontSelectionValue other) const {
  return backing_ < other.backing_;
}

inline bool FontSelectionValue::operator<=(
    const FontSelectionValue other) const {
  return backing_ <= other.backing_;
}

inline bool FontSelectionValue::operator>(
    const FontSelectionValue other) const {
  return backing_ > other.backing_;
}

inline bool FontSelectionValue::operator>=(
    const FontSelectionValue other) const {
  return backing_ >= other.backing_;
}

static inline const FontSelectionValue& ItalicThreshold() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue, italicThreshold,
                                  (20));
  return italicThreshold;
}

static inline bool isItalic(FontSelectionValue fontWeight) {
  return fontWeight >= ItalicThreshold();
}

static inline const FontSelectionValue& FontSelectionZeroValue() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue,
                                  fontSelectionZeroValue, (0));
  return fontSelectionZeroValue;
}

static inline const FontSelectionValue& NormalSlopeValue() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue, normalSlopeValue,
                                  ());
  return normalSlopeValue;
}

static inline const FontSelectionValue& ItalicSlopeValue() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue, italicValue, (20));
  return italicValue;
}

static inline const FontSelectionValue& MaxObliqueValue() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue, maxObliqueValue,
                                  (90));
  return maxObliqueValue;
}

static inline const FontSelectionValue& MinObliqueValue() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue, minObliqueValue,
                                  (-90));
  return minObliqueValue;
}

static inline const FontSelectionValue& BoldThreshold() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue, boldThreshold,
                                  (600));
  return boldThreshold;
}

static inline const FontSelectionValue& MinWeightValue() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue, minWeightValue,
                                  (1));
  return minWeightValue;
}

static inline const FontSelectionValue& MaxWeightValue() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue, maxWeightValue,
                                  (1000));
  return maxWeightValue;
}

static inline const FontSelectionValue& BoldWeightValue() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue, boldWeightValue,
                                  (700));
  return boldWeightValue;
}

static inline const FontSelectionValue& NormalWeightValue() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue, normalWeightValue,
                                  (400));
  return normalWeightValue;
}

static inline const FontSelectionValue& LightWeightValue() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue, lightWeightValue,
                                  (200));
  return lightWeightValue;
}

static inline bool isFontWeightBold(FontSelectionValue fontWeight) {
  return fontWeight >= BoldThreshold();
}

static inline const FontSelectionValue& UpperWeightSearchThreshold() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue,
                                  upperWeightSearchThreshold, (500));
  return upperWeightSearchThreshold;
}

static inline const FontSelectionValue& LowerWeightSearchThreshold() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue,
                                  lowerWeightSearchThreshold, (400));
  return lowerWeightSearchThreshold;
}

static inline const FontSelectionValue& UltraCondensedWidthValue() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue,
                                  ultraCondensedWidthValue, (50));
  return ultraCondensedWidthValue;
}

static inline const FontSelectionValue& ExtraCondensedWidthValue() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue,
                                  extraCondensedWidthValue, (62.5f));
  return extraCondensedWidthValue;
}

static inline const FontSelectionValue& CondensedWidthValue() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue, condensedWidthValue,
                                  (75));
  return condensedWidthValue;
}

static inline const FontSelectionValue& SemiCondensedWidthValue() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue,
                                  semiCondensedWidthValue, (87.5f));
  return semiCondensedWidthValue;
}

static inline const FontSelectionValue& NormalWidthValue() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue, normalWidthValue,
                                  (100.0f));
  return normalWidthValue;
}

static inline const FontSelectionValue& SemiExpandedWidthValue() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue,
                                  semiExpandedWidthValue, (112.5f));
  return semiExpandedWidthValue;
}

static inline const FontSelectionValue& ExpandedWidthValue() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue, expandedWidthValue,
                                  (125));
  return expandedWidthValue;
}

static inline const FontSelectionValue& ExtraExpandedWidthValue() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue,
                                  extraExpandedWidthValue, (150));
  return extraExpandedWidthValue;
}

static inline const FontSelectionValue& UltraExpandedWidthValue() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontSelectionValue,
                                  ultraExpandedWidthValue, (200));
  return ultraExpandedWidthValue;
}

struct FontSelectionRange {
  FontSelectionRange(FontSelectionValue single_value)
      : minimum(single_value), maximum(single_value) {}

  FontSelectionRange(FontSelectionValue minimum, FontSelectionValue maximum)
      : minimum(minimum), maximum(maximum) {}

  bool operator==(const FontSelectionRange& other) const {
    return minimum == other.minimum && maximum == other.maximum;
  }

  bool IsValid() const { return minimum <= maximum; }

  bool IsRange() const { return maximum > minimum; }

  void Expand(const FontSelectionRange& other) {
    DCHECK(other.IsValid());
    if (!IsValid()) {
      *this = other;
    } else {
      minimum = std::min(minimum, other.minimum);
      maximum = std::max(maximum, other.maximum);
    }
    DCHECK(IsValid());
  }

  bool Includes(FontSelectionValue target) const {
    return target >= minimum && target <= maximum;
  }

  uint32_t UniqueValue() const {
    return minimum.RawValue() << 16 | maximum.RawValue();
  }

  FontSelectionValue clampToRange(FontSelectionValue selection_value) const {
    if (selection_value < minimum)
      return minimum;
    if (selection_value > maximum)
      return maximum;
    return selection_value;
  }

  FontSelectionValue minimum{FontSelectionValue(1)};
  FontSelectionValue maximum{FontSelectionValue(0)};
};

struct PLATFORM_EXPORT FontSelectionRequest {
  FontSelectionRequest() = default;

  FontSelectionRequest(FontSelectionValue weight,
                       FontSelectionValue width,
                       FontSelectionValue slope)
      : weight(weight), width(width), slope(slope) {}

  unsigned GetHash() const;

  bool operator==(const FontSelectionRequest& other) const {
    return weight == other.weight && width == other.width &&
           slope == other.slope;
  }

  bool operator!=(const FontSelectionRequest& other) const {
    return !operator==(other);
  }

  String ToString() const;

  FontSelectionValue weight;
  FontSelectionValue width;
  FontSelectionValue slope;
};

// Only used for HashMaps. We don't want to put the bool into
// FontSelectionRequest because FontSelectionRequest needs to be as small as
// possible because it's inside every FontDescription.
struct FontSelectionRequestKey {
  FontSelectionRequestKey() = default;

  FontSelectionRequestKey(FontSelectionRequest request) : request(request) {}

  explicit FontSelectionRequestKey(WTF::HashTableDeletedValueType)
      : isDeletedValue(true) {}

  bool IsHashTableDeletedValue() const { return isDeletedValue; }

  bool operator==(const FontSelectionRequestKey& other) const {
    return request == other.request && isDeletedValue == other.isDeletedValue;
  }

  FontSelectionRequest request;
  bool isDeletedValue{false};
};

struct PLATFORM_EXPORT FontSelectionRequestKeyHash {
  static unsigned GetHash(const FontSelectionRequestKey&);

  static bool Equal(const FontSelectionRequestKey& a,
                    const FontSelectionRequestKey& b) {
    return a == b;
  }

  static const bool safe_to_compare_to_empty_or_deleted = true;
};

struct FontSelectionCapabilities {
  FontSelectionCapabilities() = default;

  FontSelectionCapabilities(FontSelectionRange width,
                            FontSelectionRange slope,
                            FontSelectionRange weight)
      : width(width), slope(slope), weight(weight), is_deleted_value_(false) {}

  FontSelectionCapabilities(WTF::HashTableDeletedValueType)
      : is_deleted_value_(true) {}

  bool IsHashTableDeletedValue() const { return is_deleted_value_; }

  void Expand(const FontSelectionCapabilities& capabilities) {
    width.Expand(capabilities.width);
    slope.Expand(capabilities.slope);
    weight.Expand(capabilities.weight);
  }

  bool IsValid() const {
    return width.IsValid() && slope.IsValid() && weight.IsValid() &&
           !is_deleted_value_;
  }

  bool HasRange() const {
    return width.IsRange() || slope.IsRange() || weight.IsRange();
  }

  bool operator==(const FontSelectionCapabilities& other) const {
    return width == other.width && slope == other.slope &&
           weight == other.weight &&
           is_deleted_value_ == other.is_deleted_value_;
  }

  bool operator!=(const FontSelectionCapabilities& other) const {
    return !(*this == other);
  }

  FontSelectionRange width{FontSelectionZeroValue(), FontSelectionZeroValue()};
  FontSelectionRange slope{FontSelectionZeroValue(), FontSelectionZeroValue()};
  FontSelectionRange weight{FontSelectionZeroValue(), FontSelectionZeroValue()};
  bool is_deleted_value_{false};
};

struct PLATFORM_EXPORT FontSelectionCapabilitiesHash {
  static unsigned GetHash(const FontSelectionCapabilities& key);

  static bool Equal(const FontSelectionCapabilities& a,
                    const FontSelectionCapabilities& b) {
    return a == b;
  }

  static const bool safe_to_compare_to_empty_or_deleted = true;
};

}  // namespace blink

namespace WTF {

template <>
struct DefaultHash<blink::FontSelectionCapabilities> {
  STATIC_ONLY(DefaultHash);
  typedef blink::FontSelectionCapabilitiesHash Hash;
};

template <>
struct HashTraits<blink::FontSelectionCapabilities>
    : SimpleClassHashTraits<blink::FontSelectionCapabilities> {
  STATIC_ONLY(HashTraits);
};

}  // namespace WTF

// Used for clampTo for example in StyleBuilderConverter
template <>
inline blink::FontSelectionValue
defaultMinimumForClamp<blink::FontSelectionValue>() {
  return blink::FontSelectionValue::MinimumValue();
}

template <>
inline blink::FontSelectionValue
defaultMaximumForClamp<blink::FontSelectionValue>() {
  return blink::FontSelectionValue::MaximumValue();
}

#endif
