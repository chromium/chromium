// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/font_features.h"

#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"

namespace blink {

namespace {

constexpr hb_feature_t CreateFeature(hb_tag_t tag, uint32_t value = 0) {
  return {tag, value, 0 /* start */, static_cast<unsigned>(-1) /* end */};
}

constexpr hb_feature_t CreateFeature(char c1,
                                     char c2,
                                     char c3,
                                     char c4,
                                     uint32_t value = 0) {
  return CreateFeature(HB_TAG(c1, c2, c3, c4), value);
}

}  // namespace

std::optional<unsigned> FontFeatures::FindValueForTesting(hb_tag_t tag) const {
  for (const hb_feature_t& feature : features_) {
    if (feature.tag == tag)
      return feature.value;
  }
  return std::nullopt;
}

void FontFeatures::Initialize(const FontDescription& description) {
  DCHECK(IsEmpty());
  const bool is_horizontal = !description.IsVerticalAnyUpright();

  constexpr hb_feature_t no_kern = CreateFeature('k', 'e', 'r', 'n');
  constexpr hb_feature_t no_vkrn = CreateFeature('v', 'k', 'r', 'n');
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
    constexpr hb_feature_t no_clig = CreateFeature('c', 'l', 'i', 'g');
    constexpr hb_feature_t no_liga = CreateFeature('l', 'i', 'g', 'a');
    auto common = description.CommonLigaturesState();
    if (letter_spacing ||
        (common == disabled || (common == normal && default_is_off))) {
      Append(no_liga);
      Append(no_clig);
    }
    // dlig is off by default in HarfBuzz
    constexpr hb_feature_t dlig = CreateFeature('d', 'l', 'i', 'g', 1);
    auto discretionary = description.DiscretionaryLigaturesState();
    if (!letter_spacing && discretionary == enabled) {
      Append(dlig);
    }
    // hlig is off by default in HarfBuzz
    constexpr hb_feature_t hlig = CreateFeature('h', 'l', 'i', 'g', 1);
    auto historical = description.HistoricalLigaturesState();
    if (!letter_spacing && historical == enabled) {
      Append(hlig);
    }
    // calt is on by default in HarfBuzz
    constexpr hb_feature_t no_calt = CreateFeature('c', 'a', 'l', 't');
    auto contextual = description.ContextualLigaturesState();
    if (letter_spacing ||
        (contextual == disabled || (contextual == normal && default_is_off))) {
      Append(no_calt);
    }
  }

  static constexpr hb_feature_t hwid = CreateFeature('h', 'w', 'i', 'd', 1);
  static constexpr hb_feature_t twid = CreateFeature('t', 'w', 'i', 'd', 1);
  static constexpr hb_feature_t qwid = CreateFeature('q', 'w', 'i', 'd', 1);
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
    static constexpr hb_feature_t jp78 = CreateFeature('j', 'p', '7', '8', 1);
    static constexpr hb_feature_t jp83 = CreateFeature('j', 'p', '8', '3', 1);
    static constexpr hb_feature_t jp90 = CreateFeature('j', 'p', '9', '0', 1);
    static constexpr hb_feature_t jp04 = CreateFeature('j', 'p', '0', '4', 1);
    static constexpr hb_feature_t smpl = CreateFeature('s', 'm', 'p', 'l', 1);
    static constexpr hb_feature_t trad = CreateFeature('t', 'r', 'a', 'd', 1);
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
        NOTREACHED_IN_MIGRATION();
    }
    static constexpr hb_feature_t fwid = CreateFeature('f', 'w', 'i', 'd', 1);
    static constexpr hb_feature_t pwid = CreateFeature('p', 'w', 'i', 'd', 1);
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
        NOTREACHED_IN_MIGRATION();
    }
    static constexpr hb_feature_t ruby = CreateFeature('r', 'u', 'b', 'y', 1);
    if (east_asian.Ruby())
      Append(ruby);
  }

  // font-variant-numeric:
  static constexpr hb_feature_t lnum = CreateFeature('l', 'n', 'u', 'm', 1);
  if (description.VariantNumeric().NumericFigureValue() ==
      FontVariantNumeric::kLiningNums)
    Append(lnum);

  static constexpr hb_feature_t onum = CreateFeature('o', 'n', 'u', 'm', 1);
  if (description.VariantNumeric().NumericFigureValue() ==
      FontVariantNumeric::kOldstyleNums)
    Append(onum);

  static constexpr hb_feature_t pnum = CreateFeature('p', 'n', 'u', 'm', 1);
  if (description.VariantNumeric().NumericSpacingValue() ==
      FontVariantNumeric::kProportionalNums)
    Append(pnum);
  static constexpr hb_feature_t tnum = CreateFeature('t', 'n', 'u', 'm', 1);
  if (description.VariantNumeric().NumericSpacingValue() ==
      FontVariantNumeric::kTabularNums)
    Append(tnum);

  static constexpr hb_feature_t afrc = CreateFeature('a', 'f', 'r', 'c', 1);
  if (description.VariantNumeric().NumericFractionValue() ==
      FontVariantNumeric::kStackedFractions)
    Append(afrc);
  static constexpr hb_feature_t frac = CreateFeature('f', 'r', 'a', 'c', 1);
  if (description.VariantNumeric().NumericFractionValue() ==
      FontVariantNumeric::kDiagonalFractions)
    Append(frac);

  static constexpr hb_feature_t ordn = CreateFeature('o', 'r', 'd', 'n', 1);
  if (description.VariantNumeric().OrdinalValue() ==
      FontVariantNumeric::kOrdinalOn)
    Append(ordn);

  static constexpr hb_feature_t zero = CreateFeature('z', 'e', 'r', 'o', 1);
  if (description.VariantNumeric().SlashedZeroValue() ==
      FontVariantNumeric::kSlashedZeroOn)
    Append(zero);

  const hb_tag_t chws_or_vchw =
      is_horizontal ? HB_TAG('c', 'h', 'w', 's') : HB_TAG('v', 'c', 'h', 'w');
  bool default_enable_chws =
      ShouldTrimAdjacent(description.GetTextSpacingTrim());

  const FontFeatureSettings* settings = description.FeatureSettings();
  if (settings) [[unlikely]] {
    // TODO(drott): crbug.com/450619 Implement feature resolution instead of
    // just appending the font-feature-settings.
    const hb_tag_t halt_or_vhal =
        is_horizontal ? HB_TAG('h', 'a', 'l', 't') : HB_TAG('v', 'h', 'a', 'l');
    const hb_tag_t palt_or_vpal =
        is_horizontal ? HB_TAG('p', 'a', 'l', 't') : HB_TAG('v', 'p', 'a', 'l');
    for (const FontFeature& setting : *settings) {
      const hb_feature_t feature =
          CreateFeature(setting.Tag(), setting.Value());
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
    Append(CreateFeature(chws_or_vchw, 1));

  const FontDescription::FontVariantPosition variant_position =
      description.VariantPosition();
  if (variant_position == FontDescription::kSubVariantPosition) {
    const hb_feature_t feature = CreateFeature('s', 'u', 'b', 's', 1);
    Append(feature);
  }
  if (variant_position == FontDescription::kSuperVariantPosition) {
    const hb_feature_t feature = CreateFeature('s', 'u', 'p', 's', 1);
    Append(feature);
  }
}

}  // namespace blink
