// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_color_mix_value.h"

#include <optional>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink::cssvalue {
namespace {

using Percentages = Vector<std::optional<double>>;

const CSSPrimitiveValue* ToCSSPercentage(std::optional<double> percent) {
  return percent ? CSSNumericLiteralValue::Create(
                       *percent, CSSPrimitiveValue::UnitType::kPercentage)
                 : nullptr;
}

const CSSValue* ParseColor(const char* text) {
  return CSSParser::ParseSingleValue(
      CSSPropertyID::kColor, String(text),
      StrictCSSParserContext(SecureContextMode::kInsecureContext));
}

// Lists of two percentages are tested by TwoItemsMatchTwoColorNormalization,
// and lists of omitted percentages by NormalizeMixPercentagesAllOmitted.
TEST(CSSColorMixValueTest, NormalizeMixPercentages) {
  struct {
    Percentages input;
    Vector<double> expected_percentages;
    double expected_alpha_multiplier;
  } tests[] = {
      {{0.5}, {0.5}, 0.5},
      {{0.5, std::nullopt, std::nullopt}, {0.5, 0.25, 0.25}, 1.0},
      {{0.3, 0.6, 0.9}, {0.3 / 1.8, 0.6 / 1.8, 0.9 / 1.8}, 1.0},
      // The omitted percentage can't be negative.
      {{0.7, 0.5, std::nullopt}, {0.7 / 1.2, 0.5 / 1.2, 0.0}, 1.0},
      {{0.1, 0.2, 0.3}, {0.1, 0.2, 0.3}, 0.1 + 0.2 + 0.3},
      {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, 0.0},
  };
  for (const auto& test : tests) {
    const CSSColorMixValue::NormalizedPercentages result =
        CSSColorMixValue::NormalizeMixPercentages(test.input);
    ASSERT_EQ(result.percentages.size(), test.expected_percentages.size());
    for (wtf_size_t i = 0; i < result.percentages.size(); ++i) {
      EXPECT_DOUBLE_EQ(result.percentages[i], test.expected_percentages[i])
          << "item " << i << " of a list of " << test.input.size();
    }
    EXPECT_DOUBLE_EQ(result.alpha_multiplier, test.expected_alpha_multiplier);
  }
}

TEST(CSSColorMixValueTest, NormalizeMixPercentagesAllOmitted) {
  // See comment in NormalizeMixPercentages().
  // Validates that if all percentages are omitted, they are all assigned
  // an equal value, even if they don't all add up to 1.
  for (wtf_size_t count = 1; count <= 20; ++count) {
    const CSSColorMixValue::NormalizedPercentages result =
        CSSColorMixValue::NormalizeMixPercentages(
            Percentages(count, std::nullopt));
    EXPECT_EQ(result.alpha_multiplier, 1.0) << count;
    for (double percentage : result.percentages) {
      EXPECT_EQ(percentage, 1.0 / count) << count;
    }
  }
}

// The two-color percentage normalization that color-mix() used before it
// accepted a list of colors, as a reference.
std::pair<double, double> ReferenceMixAmountAndAlphaMultiplier(
    std::optional<double> p1,
    std::optional<double> p2) {
  double percentage1 = p1.value_or(p2 ? 1.0 - *p2 : 0.5);
  double percentage2 = p2.value_or(p1 ? 1.0 - *p1 : 0.5);
  const double sum = percentage1 + percentage2;
  if (sum == 0.0) {
    return {0.5, 0.0};
  }
  return {percentage2 / sum, sum <= 1.0 ? sum : 1.0};
}

TEST(CSSColorMixValueTest, TwoItemsMatchTwoColorNormalization) {
  const CSSToLengthConversionData length_resolver(/*element=*/nullptr);
  const struct {
    Color::ColorSpace space;
    Color color1;
    Color color2;
  } mixes[] = {
      {Color::ColorSpace::kSRGB, Color::FromRGBA(255, 0, 0, 200),
       Color::FromRGB(0, 0, 255)},
      {Color::ColorSpace::kOklch,
       Color::FromColorSpace(Color::ColorSpace::kLab, 50.0f, 40.0f, -20.0f,
                             1.0f),
       Color::FromColorSpace(Color::ColorSpace::kOklch, 0.7f, 0.1f, 200.0f,
                             0.5f)},
      {Color::ColorSpace::kHSL, Color::FromRGB(0, 128, 0),
       Color::FromColorSpace(Color::ColorSpace::kHWB, 300.0f, 0.2f, 0.3f,
                             1.0f)},
  };
  Vector<std::optional<int>> percents = {std::nullopt};
  for (int percent = 0; percent <= 100; ++percent) {
    percents.push_back(percent);
  }
  auto to_fraction = [](std::optional<int> percent) -> std::optional<double> {
    return percent ? std::optional<double>(*percent / 100.0) : std::nullopt;
  };

  for (const auto& mix : mixes) {
    for (std::optional<int> percent1 : percents) {
      for (std::optional<int> percent2 : percents) {
        SCOPED_TRACE(testing::Message() << percent1.value_or(-1) << "% "
                                        << percent2.value_or(-1) << "%");
        const std::optional<double> p1 = to_fraction(percent1);
        const std::optional<double> p2 = to_fraction(percent2);
        const auto [reference_mix_amount, reference_alpha_multiplier] =
            ReferenceMixAmountAndAlphaMultiplier(p1, p2);
        const Color expected = Color::FromColorMix(
            mix.space, std::nullopt, mix.color1, mix.color2,
            reference_mix_amount, reference_alpha_multiplier);

        const CSSColorMixValue::NormalizedPercentages normalized =
            CSSColorMixValue::NormalizeMixPercentages(Percentages{p1, p2});
        EXPECT_EQ(Color::FromColorMix(
                      mix.space, std::nullopt, {mix.color1, mix.color2},
                      normalized.percentages, normalized.alpha_multiplier),
                  expected);

        double mix_amount;
        double alpha_multiplier;
        CSSColorMixValue::NormalizePercentages(
            ToCSSPercentage(percent1), ToCSSPercentage(percent2), mix_amount,
            alpha_multiplier, length_resolver);
        EXPECT_EQ(Color::FromColorMix(mix.space, std::nullopt, mix.color1,
                                      mix.color2, mix_amount, alpha_multiplier),
                  expected);
      }
    }
  }
}

TEST(CSSColorMixValueTest, PercentageValuesForSerialization) {
  constexpr std::nullopt_t kOmitted = std::nullopt;
  struct {
    Percentages input;
    Vector<String> expected;  // A null string means no percentage.
  } tests[] = {
      {{kOmitted, kOmitted, kOmitted}, {String(), String(), String()}},
      {{50, 50}, {String(), String()}},
      {{25, kOmitted, kOmitted, kOmitted},
       {String(), String(), String(), String()}},
      // Equal percentages above 100% / N are kept, as other engines do.
      {{75, 75}, {"75%", "75%"}},
      {{50, kOmitted, kOmitted}, {"50%", "25%", "25%"}},
      // The omitted percentage would be negative, so it's clamped to 0.
      {{70, 50, kOmitted}, {"70%", "50%", "0%"}},
  };
  for (const auto& test : tests) {
    const CSSColorMixValue::PercentageVector result =
        CSSColorMixValue::PercentageValuesForSerialization(
            CSSColorMixValue::PercentageVector(test.input, &ToCSSPercentage));
    ASSERT_EQ(result.size(), test.expected.size());
    for (wtf_size_t i = 0; i < result.size(); ++i) {
      EXPECT_EQ(result[i] ? result[i]->CssText() : String(), test.expected[i])
          << "item " << i << " of a list of " << test.input.size();
    }
  }
}

TEST(CSSColorMixValueTest, OneOrMoreColorsDisabled) {
  ScopedCSSColorMixVariadicForTest scoped_feature(false);
  EXPECT_FALSE(ParseColor("color-mix(in srgb, red)"));
  EXPECT_FALSE(ParseColor("color-mix(red)"));
  EXPECT_FALSE(ParseColor("color-mix(in srgb, red, green, blue)"));
  EXPECT_TRUE(ParseColor("color-mix(in srgb, red, blue)"));
  EXPECT_TRUE(ParseColor("color-mix(red 20%, blue)"));
}

}  // namespace
}  // namespace blink::cssvalue
