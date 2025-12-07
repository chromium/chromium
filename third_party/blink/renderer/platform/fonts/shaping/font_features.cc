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

bool FontFeatureRange::IsInitial(base::span<const FontFeatureRange> features) {
  return features.size() == 1 && features[0] == kChws;
}

template <wtf_size_t InlineCapacity>
void FontFeatureRange::FromFontDescription(
    const FontDescription& description,
    Vector<FontFeatureRange, InlineCapacity>& features) {
  DCHECK(features.empty());
  const bool is_horizontal = !description.IsVerticalAnyUpright();

  constexpr FontFeatureRange no_kern{{{'k', 'e', 'r', 'n'}}};
  constexpr FontFeatureRange no_vkrn{{{'v', 'k', 'r', 'n'}}};
  switch (description.GetKerning()) {
    case FontDescription::kNormalKerning:
      // kern/vkrn are enabled by default in HarfBuzz
      break;
    case FontDescription::kNoneKerning:
      features.push_back(is_horizontal ? no_kern : no_vkrn);
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
      features.push_back(no_liga);
      features.push_back(no_clig);
    }
    // dlig is off by default in HarfBuzz
    constexpr FontFeatureRange dlig{{{'d', 'l', 'i', 'g'}, 1}};
    auto discretionary = description.DiscretionaryLigaturesState();
    if (!letter_spacing && discretionary == enabled) {
      features.push_back(dlig);
    }
    // hlig is off by default in HarfBuzz
    constexpr FontFeatureRange hlig{{{'h', 'l', 'i', 'g'}, 1}};
    auto historical = description.HistoricalLigaturesState();
    if (!letter_spacing && historical == enabled) {
      features.push_back(hlig);
    }
    // calt is on by default in HarfBuzz
    constexpr FontFeatureRange no_calt{{{'c', 'a', 'l', 't'}}};
    auto contextual = description.ContextualLigaturesState();
    if (letter_spacing ||
        (contextual == disabled || (contextual == normal && default_is_off))) {
      features.push_back(no_calt);
    }
  }

  static constexpr FontFeatureRange hwid{{{'h', 'w', 'i', 'd'}, 1}};
  static constexpr FontFeatureRange twid{{{'t', 'w', 'i', 'd'}, 1}};
  static constexpr FontFeatureRange qwid{{{'q', 'w', 'i', 'd'}, 1}};
  switch (description.WidthVariant()) {
    case kHalfWidth:
      features.push_back(hwid);
      break;
    case kThirdWidth:
      features.push_back(twid);
      break;
    case kQuarterWidth:
      features.push_back(qwid);
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
        features.push_back(jp78);
        break;
      case FontVariantEastAsian::kJis83:
        features.push_back(jp83);
        break;
      case FontVariantEastAsian::kJis90:
        features.push_back(jp90);
        break;
      case FontVariantEastAsian::kJis04:
        features.push_back(jp04);
        break;
      case FontVariantEastAsian::kSimplified:
        features.push_back(smpl);
        break;
      case FontVariantEastAsian::kTraditional:
        features.push_back(trad);
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
        features.push_back(fwid);
        break;
      case FontVariantEastAsian::kProportionalWidth:
        features.push_back(pwid);
        break;
      default:
        NOTREACHED();
    }
    static constexpr FontFeatureRange ruby{{{'r', 'u', 'b', 'y'}, 1}};
    if (east_asian.Ruby())
      features.push_back(ruby);
  }

  // font-variant-numeric:
  static constexpr FontFeatureRange lnum{{{'l', 'n', 'u', 'm'}, 1}};
  if (description.VariantNumeric().NumericFigureValue() ==
      FontVariantNumeric::kLiningNums)
    features.push_back(lnum);

  static constexpr FontFeatureRange onum{{{'o', 'n', 'u', 'm'}, 1}};
  if (description.VariantNumeric().NumericFigureValue() ==
      FontVariantNumeric::kOldstyleNums)
    features.push_back(onum);

  static constexpr FontFeatureRange pnum{{{'p', 'n', 'u', 'm'}, 1}};
  if (description.VariantNumeric().NumericSpacingValue() ==
      FontVariantNumeric::kProportionalNums)
    features.push_back(pnum);
  static constexpr FontFeatureRange tnum{{{'t', 'n', 'u', 'm'}, 1}};
  if (description.VariantNumeric().NumericSpacingValue() ==
      FontVariantNumeric::kTabularNums)
    features.push_back(tnum);

  static constexpr FontFeatureRange afrc{{{'a', 'f', 'r', 'c'}, 1}};
  if (description.VariantNumeric().NumericFractionValue() ==
      FontVariantNumeric::kStackedFractions)
    features.push_back(afrc);
  static constexpr FontFeatureRange frac{{{'f', 'r', 'a', 'c'}, 1}};
  if (description.VariantNumeric().NumericFractionValue() ==
      FontVariantNumeric::kDiagonalFractions)
    features.push_back(frac);

  static constexpr FontFeatureRange ordn{{{'o', 'r', 'd', 'n'}, 1}};
  if (description.VariantNumeric().OrdinalValue() ==
      FontVariantNumeric::kOrdinalOn)
    features.push_back(ordn);

  static constexpr FontFeatureRange zero{{{'z', 'e', 'r', 'o'}, 1}};
  if (description.VariantNumeric().SlashedZeroValue() ==
      FontVariantNumeric::kSlashedZeroOn)
    features.push_back(zero);

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
      features.push_back(feature);

      // `chws` should not be appended if other glyph-width GPOS feature exists.
      if (default_enable_chws &&
          (feature.tag == chws_or_vchw ||
           (feature.value &&
            (feature.tag == halt_or_vhal || feature.tag == palt_or_vpal))))
        default_enable_chws = false;
    }
  }

  if (default_enable_chws)
    features.push_back(FontFeatureRange{{chws_or_vchw, 1}});

  const FontDescription::FontVariantPosition variant_position =
      description.VariantPosition();
  if (variant_position == FontDescription::kSubVariantPosition) {
    constexpr FontFeatureRange feature{{{'s', 'u', 'b', 's'}, 1}};
    features.push_back(feature);
  }
  if (variant_position == FontDescription::kSuperVariantPosition) {
    constexpr FontFeatureRange feature{{{'s', 'u', 'p', 's'}, 1}};
    features.push_back(feature);
  }
}

//
// Explicitly instantiate template functions.
//
template PLATFORM_EXPORT void FontFeatureRange::FromFontDescription(
    const FontDescription&,
    Vector<FontFeatureRange, FontFeatureRange::kInitialSize>&);
template PLATFORM_EXPORT void FontFeatureRange::FromFontDescription(
    const FontDescription&,
    FontFeatureRanges&);

#if EXPENSIVE_DCHECKS_ARE_ON()
void FontFeatureRangesSaver::CheckIsAdditionsOnly() const {
  DCHECK_GE(features_->size(), num_features_before_);
  const wtf_size_t size = std::min(features_->size(), num_features_before_);
  for (wtf_size_t i = 0; i < size; ++i) {
    DCHECK_EQ((*features_)[i], saved_features_[i]);
  }
}
#endif  // EXPENSIVE_DCHECKS_ARE_ON()

}  // namespace blink
