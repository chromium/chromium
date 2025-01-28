// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/font_features.h"

#include <hb.h>

#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"

namespace blink {

namespace {

constexpr FontFeatureRange kChws{{{'c', 'h', 'w', 's'}, 1}};

FontFeatures CreateInitial() {
  FontFeatures features;
  features.Append(kChws);
  return features;
}

}  // namespace

//
// Ensure `FontFeatureTag` is compatible with `hb_tag_t`.
//
static_assert(sizeof(FontFeatureTag) == sizeof(hb_tag_t));
static_assert(FontFeatureTag('1', '2', '3', '4').tag ==
              HB_TAG('1', '2', '3', '4'));

//
// Ensure `FontFeatureRange` is compatible with `hb_feature_t`.
//
static_assert(sizeof(FontFeatureRange) == sizeof(hb_feature_t));
static_assert(offsetof(FontFeatureRange, tag) == offsetof(hb_feature_t, tag));
static_assert(offsetof(FontFeatureRange, value) ==
              offsetof(hb_feature_t, value));
static_assert(offsetof(FontFeatureRange, start) ==
              offsetof(hb_feature_t, start));
static_assert(offsetof(FontFeatureRange, end) == offsetof(hb_feature_t, end));

const FontFeatures& FontFeatures::Initial() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(FontFeatures, initial_features,
                                  (CreateInitial()));
  return initial_features;
}

bool FontFeatures::IsInitial() const {
  return size() == 1 && (*this)[0] == kChws;
}

const hb_feature_t* FontFeatures::ToHarfBuzzData() const {
  return reinterpret_cast<const hb_feature_t*>(features_.data());
}

std::optional<uint32_t> FontFeatures::FindValueForTesting(uint32_t tag) const {
  for (const FontFeatureRange& feature : features_) {
    if (feature.tag == tag)
      return feature.value;
  }
  return std::nullopt;
}

void FontFeatures::Initialize(const FontDescription& description) {
  DCHECK(IsEmpty());
  const bool is_horizontal = !description.IsVerticalAnyUpright();

  constexpr FontFeatureRange no_kern{{{'k', 'e', 'r', 'n'}}};
  constexpr FontFeatureRange no_vkrn{{{'v', 'k', 'r', 'n'}}};
  switch (description.GetKerning()) {
    case FontDescription::kNormalKerning:
      // kern/vkrn are enabled by default in HarfBuzz
      break;
    case FontDescription::kNoneKerning:
      Append(is_horizontal ? no_kern : no_vkrn);
      break;
    case FontDescription::kAutoKerning:
      break;
  }

  {
    bool default_is_off = description.TextRendering() == blink::kOptimizeSpeed;
    bool letter_spacing = description.LetterSpacing() != 0;
    constexpr auto normal = FontDescription::kNormalLigaturesState;
    constexpr auto enabled = FontDescription::kEnabledLigaturesState;
    constexpr auto disabled = FontDescription::kDisabledLigaturesState;

    // clig and liga are on by default in HarfBuzz
    constexpr FontFeatureRange no_clig{{{'c', 'l', 'i', 'g'}}};
    constexpr FontFeatureRange no_liga{{{'l', 'i', 'g', 'a'}}};
    auto common = description.CommonLigaturesState();
    if (letter_spacing ||
        (common == disabled || (common == normal && default_is_off))) {
      Append(no_liga);
      Append(no_clig);
    }
    // dlig is off by default in HarfBuzz
    constexpr FontFeatureRange dlig{{{'d', 'l', 'i', 'g'}, 1}};
    auto discretionary = description.DiscretionaryLigaturesState();
    if (!letter_spacing && discretionary == enabled) {
      Append(dlig);
    }
    // hlig is off by default in HarfBuzz
    constexpr FontFeatureRange hlig{{{'h', 'l', 'i', 'g'}, 1}};
    auto historical = description.HistoricalLigaturesState();
    if (!letter_spacing && historical == enabled) {
      Append(hlig);
    }
    // calt is on by default in HarfBuzz
    constexpr FontFeatureRange no_calt{{{'c', 'a', 'l', 't'}}};
    auto contextual = description.ContextualLigaturesState();
    if (letter_spacing ||
        (contextual == disabled || (contextual == normal && default_is_off))) {
      Append(no_calt);
    }
  }

  static constexpr FontFeatureRange hwid{{{'h', 'w', 'i', 'd'}, 1}};
  static constexpr FontFeatureRange twid{{{'t', 'w', 'i', 'd'}, 1}};
  static constexpr FontFeatureRange qwid{{{'q', 'w', 'i', 'd'}, 1}};
  switch (description.WidthVariant()) {
    case kHalfWidth:
      Append(hwid);
      break;
    case kThirdWidth:
      Append(twid);
      break;
    case kQuarterWidth:
      Append(qwid);
      break;
    case kRegularWidth:
      break;
  }

  // font-variant-east-asian:
  const FontVariantEastAsian east_asian = description.VariantEastAsian();
  if (!east_asian.IsAllNormal()) [[unlikely]] {
    static constexpr FontFeatureRange jp78{{{'j', 'p', '7', '8'}, 1}};
    static constexpr FontFeatureRange jp83{{{'j', 'p', '8', '3'}, 1}};
    static constexpr FontFeatureRange jp90{{{'j', 'p', '9', '0'}, 1}};
    static constexpr FontFeatureRange jp04{{{'j', 'p', '0', '4'}, 1}};
    static constexpr FontFeatureRange smpl{{{'s', 'm', 'p', 'l'}, 1}};
    static constexpr FontFeatureRange trad{{{'t', 'r', 'a', 'd'}, 1}};
    switch (east_asian.Form()) {
      case FontVariantEastAsian::kNormalForm:
        break;
      case FontVariantEastAsian::kJis78:
        Append(jp78);
        break;
      case FontVariantEastAsian::kJis83:
        Append(jp83);
        break;
      case FontVariantEastAsian::kJis90:
        Append(jp90);
        break;
      case FontVariantEastAsian::kJis04:
        Append(jp04);
        break;
      case FontVariantEastAsian::kSimplified:
        Append(smpl);
        break;
      case FontVariantEastAsian::kTraditional:
        Append(trad);
        break;
      default:
        NOTREACHED();
    }
    static constexpr FontFeatureRange fwid{{{'f', 'w', 'i', 'd'}, 1}};
    static constexpr FontFeatureRange pwid{{{'p', 'w', 'i', 'd'}, 1}};
    switch (east_asian.Width()) {
      case FontVariantEastAsian::kNormalWidth:
        break;
      case FontVariantEastAsian::kFullWidth:
        Append(fwid);
        break;
      case FontVariantEastAsian::kProportionalWidth:
        Append(pwid);
        break;
      default:
        NOTREACHED();
    }
    static constexpr FontFeatureRange ruby{{{'r', 'u', 'b', 'y'}, 1}};
    if (east_asian.Ruby())
      Append(ruby);
  }

  // font-variant-numeric:
  static constexpr FontFeatureRange lnum{{{'l', 'n', 'u', 'm'}, 1}};
  if (description.VariantNumeric().NumericFigureValue() ==
      FontVariantNumeric::kLiningNums)
    Append(lnum);

  static constexpr FontFeatureRange onum{{{'o', 'n', 'u', 'm'}, 1}};
  if (description.VariantNumeric().NumericFigureValue() ==
      FontVariantNumeric::kOldstyleNums)
    Append(onum);

  static constexpr FontFeatureRange pnum{{{'p', 'n', 'u', 'm'}, 1}};
  if (description.VariantNumeric().NumericSpacingValue() ==
      FontVariantNumeric::kProportionalNums)
    Append(pnum);
  static constexpr FontFeatureRange tnum{{{'t', 'n', 'u', 'm'}, 1}};
  if (description.VariantNumeric().NumericSpacingValue() ==
      FontVariantNumeric::kTabularNums)
    Append(tnum);

  static constexpr FontFeatureRange afrc{{{'a', 'f', 'r', 'c'}, 1}};
  if (description.VariantNumeric().NumericFractionValue() ==
      FontVariantNumeric::kStackedFractions)
    Append(afrc);
  static constexpr FontFeatureRange frac{{{'f', 'r', 'a', 'c'}, 1}};
  if (description.VariantNumeric().NumericFractionValue() ==
      FontVariantNumeric::kDiagonalFractions)
    Append(frac);

  static constexpr FontFeatureRange ordn{{{'o', 'r', 'd', 'n'}, 1}};
  if (description.VariantNumeric().OrdinalValue() ==
      FontVariantNumeric::kOrdinalOn)
    Append(ordn);

  static constexpr FontFeatureRange zero{{{'z', 'e', 'r', 'o'}, 1}};
  if (description.VariantNumeric().SlashedZeroValue() ==
      FontVariantNumeric::kSlashedZeroOn)
    Append(zero);

  const FontFeatureTag chws_or_vchw = is_horizontal
                                          ? FontFeatureTag{'c', 'h', 'w', 's'}
                                          : FontFeatureTag{'v', 'c', 'h', 'w'};
  bool default_enable_chws =
      ShouldTrimAdjacent(description.GetTextSpacingTrim());

  const FontFeatureSettings* settings = description.FeatureSettings();
  if (settings) [[unlikely]] {
    // TODO(drott): crbug.com/450619 Implement feature resolution instead of
    // just appending the font-feature-settings.
    const FontFeatureTag halt_or_vhal =
        is_horizontal ? FontFeatureTag{'h', 'a', 'l', 't'}
                      : FontFeatureTag{'v', 'h', 'a', 'l'};
    const FontFeatureTag palt_or_vpal =
        is_horizontal ? FontFeatureTag{'p', 'a', 'l', 't'}
                      : FontFeatureTag{'v', 'p', 'a', 'l'};
    for (const FontFeature& setting : *settings) {
      const FontFeatureRange feature{
          {setting.Tag(), static_cast<uint32_t>(setting.Value())}};
      Append(feature);

      // `chws` should not be appended if other glyph-width GPOS feature exists.
      if (default_enable_chws &&
          (feature.tag == chws_or_vchw ||
           (feature.value &&
            (feature.tag == halt_or_vhal || feature.tag == palt_or_vpal))))
        default_enable_chws = false;
    }
  }

  if (default_enable_chws)
    Append({{chws_or_vchw, 1}});

  const FontDescription::FontVariantPosition variant_position =
      description.VariantPosition();
  if (variant_position == FontDescription::kSubVariantPosition) {
    constexpr FontFeatureRange feature{{{'s', 'u', 'b', 's'}, 1}};
    Append(feature);
  }
  if (variant_position == FontDescription::kSuperVariantPosition) {
    constexpr FontFeatureRange feature{{{'s', 'u', 'p', 's'}, 1}};
    Append(feature);
  }
}

}  // namespace blink
