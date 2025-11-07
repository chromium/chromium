// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/fuzztest_utils/css_domains.h"

#include <string>

#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_value_id_mappings.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/testing/fuzztest_utils/common_domains.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace blink {

namespace {

template <typename E>
std::string CSSEnumToString(int val) {
  E enum_val = static_cast<E>(val);
  return base::ToString(enum_val);
}

}  // namespace

fuzztest::Domain<CSSPropertyID> AnyCSSProperty() {
  return fuzztest::Map(
      [](int val) { return static_cast<CSSPropertyID>(val); },
      fuzztest::InRange(kIntFirstCSSProperty, kIntLastCSSProperty));
}

fuzztest::Domain<CSSValueID> AnyCSSValue() {
  return fuzztest::Map([](int val) { return static_cast<CSSValueID>(val); },
                       fuzztest::InRange(1, kNumCSSValueKeywords - 1));
}

fuzztest::Domain<std::string> AnyCSSDisplayValue() {
  return fuzztest::Map(
      CSSEnumToString<EDisplay>,
      fuzztest::InRange(0, static_cast<int>(EDisplay::kMaxEnumValue)));
}

fuzztest::Domain<std::string> AnyCSSPositionValue() {
  return fuzztest::Map(
      CSSEnumToString<EPosition>,
      fuzztest::InRange(0, static_cast<int>(EPosition::kMaxEnumValue)));
}

fuzztest::Domain<std::string> AnyCSSVisibilityValue() {
  return fuzztest::Map(
      CSSEnumToString<EVisibility>,
      fuzztest::InRange(0, static_cast<int>(EVisibility::kMaxEnumValue)));
}

fuzztest::Domain<std::string> AnyCSSContentVisibilityValue() {
  return fuzztest::Map(
      CSSEnumToString<EContentVisibility>,
      fuzztest::InRange(0,
                        static_cast<int>(EContentVisibility::kMaxEnumValue)));
}

fuzztest::Domain<std::string> AnyCSSOverflowValue() {
  return fuzztest::Map(
      CSSEnumToString<EOverflow>,
      fuzztest::InRange(0, static_cast<int>(EOverflow::kMaxEnumValue)));
}

fuzztest::Domain<std::string> AnyCSSTextOrientationValue() {
  return fuzztest::Map(
      CSSEnumToString<ETextOrientation>,
      fuzztest::InRange(0, static_cast<int>(ETextOrientation::kMaxEnumValue)));
}

fuzztest::Domain<std::string> AnyCSSTextOverflowValue() {
  return fuzztest::OneOf(fuzztest::ElementOf<std::string>({"clip", "ellipsis"}),
                         fuzztest::Arbitrary<std::string>());
}

fuzztest::Domain<std::string> AnyPlausibleValueForCSSProperty(
    CSSPropertyID property) {
  if (property == CSSPropertyID::kDisplay) {
    return AnyCSSDisplayValue();
  }
  if (property == CSSPropertyID::kPosition) {
    return AnyCSSPositionValue();
  }
  if (property == CSSPropertyID::kVisibility) {
    return AnyCSSVisibilityValue();
  }
  if (property == CSSPropertyID::kTextOverflow) {
    return AnyCSSTextOverflowValue();
  }
  if (property == CSSPropertyID::kContentVisibility) {
    return AnyCSSContentVisibilityValue();
  }
  if (property == CSSPropertyID::kOverflowX ||
      property == CSSPropertyID::kOverflowY) {
    return AnyCSSOverflowValue();
  }
  if (property == CSSPropertyID::kTextOrientation) {
    return AnyCSSTextOrientationValue();
  }
  if (property == CSSPropertyID::kColor ||
      property == CSSPropertyID::kBackgroundColor ||
      property == CSSPropertyID::kBorderColor ||
      property == CSSPropertyID::kOutlineColor) {
    return AnyColorValue();
  }

  // For properties we haven't specifically handled, use any valid CSS value
  return fuzztest::Map(
      [](CSSValueID val) { return std::string(GetCSSValueName(val)); },
      AnyCSSValue());
}

fuzztest::Domain<std::string> AnyValueForCSSProperty(CSSPropertyID property) {
  return fuzztest::OneOf(AnyPlausibleValueForCSSProperty(property),
                         fuzztest::Arbitrary<std::string>());
}

fuzztest::Domain<std::string> AnyCSSPropertyNameValuePair() {
  return fuzztest::FlatMap(
      [](CSSPropertyID property) {
        return fuzztest::Map(
            [property](const std::string& value) {
              CSSPropertyName prop_name(property);
              const std::string prop_name_str(
                  prop_name.ToAtomicString().Utf8());
              return base::StrCat({prop_name_str, ": ", value, ";"});
            },
            AnyValueForCSSProperty(property));
      },
      AnyCSSProperty());
}

fuzztest::Domain<std::string> AnyCssDeclaration() {
  return fuzztest::Map(
      [](base::span<const std::string> properties) {
        return base::JoinString(properties, " ");
      },
      fuzztest::VectorOf(AnyCSSPropertyNameValuePair()).WithMaxSize(3));
}

}  // namespace blink
