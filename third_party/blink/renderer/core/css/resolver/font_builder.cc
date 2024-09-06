/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2015 Collabora Ltd. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/css/resolver/font_builder.h"

#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"

namespace blink {

FontBuilder::FontBuilder(Document* document) : document_(document) {
  DCHECK(!document || document->GetFrame());
}

void FontBuilder::DidChangeEffectiveZoom() {
  Set(PropertySetFlag::kEffectiveZoom);
}

void FontBuilder::DidChangeTextOrientation() {
  Set(PropertySetFlag::kTextOrientation);
}

void FontBuilder::DidChangeWritingMode() {
  Set(PropertySetFlag::kWritingMode);
}

void FontBuilder::DidChangeTextSizeAdjust() {
  // When `TextSizeAdjustImprovements` is enabled, text-size-adjust affects
  // font-size during style building, and needs to invalidate the font
  // description.
  if (RuntimeEnabledFeatures::TextSizeAdjustImprovementsEnabled()) {
    Set(PropertySetFlag::kTextSizeAdjust);
  }
}

FontFamily FontBuilder::StandardFontFamily() const {
  const AtomicString& standard_font_family = StandardFontFamilyName();
  return FontFamily(standard_font_family,
                    FontFamily::InferredTypeFor(standard_font_family));
}

AtomicString FontBuilder::StandardFontFamilyName() const {
  if (document_) {
    Settings* settings = document_->GetSettings();
    if (settings) {
      return settings->GetGenericFontFamilySettings().Standard();
    }
  }
  return AtomicString();
}

AtomicString FontBuilder::GenericFontFamilyName(
    FontDescription::GenericFamilyType generic_family) const {
  switch (generic_family) {
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case FontDescription::kNoFamily:
      return AtomicString();
    // While the intention is to phase out kWebkitBodyFamily, it should still
    // map to the standard font from user preference.
    case FontDescription::kWebkitBodyFamily:
      return StandardFontFamilyName();
    case FontDescription::kSerifFamily:
      return font_family_names::kSerif;
    case FontDescription::kSansSerifFamily:
      return font_family_names::kSansSerif;
    case FontDescription::kMonospaceFamily:
      return font_family_names::kMonospace;
    case FontDescription::kCursiveFamily:
      return font_family_names::kCursive;
    case FontDescription::kFantasyFamily:
      return font_family_names::kFantasy;
  }
}

float FontBuilder::FontSizeForKeyword(unsigned keyword,
                                      bool is_monospace) const {
  return FontSizeFunctions::FontSizeForKeyword(document_, keyword,
                                               is_monospace);
}

void FontBuilder::SetFamilyDescription(
    const FontDescription::FamilyDescription& family_description) {
  SetFamilyDescription(font_description_, family_description);
}

void FontBuilder::SetFamilyTreeScope(const TreeScope* tree_scope) {
  family_tree_scope_ = tree_scope;
}

void FontBuilder::SetWeight(FontSelectionValue weight) {
  Set(PropertySetFlag::kWeight);

  font_description_.SetWeight(weight);
}

void FontBuilder::SetStyle(FontSelectionValue slope) {
  Set(PropertySetFlag::kStyle);

  font_description_.SetStyle(slope);
}

void FontBuilder::SetStretch(FontSelectionValue stretch) {
  Set(PropertySetFlag::kStretch);

  font_description_.SetStretch(stretch);
}

void FontBuilder::SetSize(const FontDescription::Size& size) {
  SetSize(font_description_, size);
}

void FontBuilder::SetSizeAdjust(const FontSizeAdjust& size_adjust) {
  Set(PropertySetFlag::kSizeAdjust);

  font_description_.SetSizeAdjust(size_adjust);
}

void FontBuilder::SetLocale(scoped_refptr<const LayoutLocale> locale) {
  Set(PropertySetFlag::kLocale);

  font_description_.SetLocale(std::move(locale));
}

void FontBuilder::SetVariantCaps(FontDescription::FontVariantCaps caps) {
  Set(PropertySetFlag::kVariantCaps);

  font_description_.SetVariantCaps(caps);
}

void FontBuilder::SetVariantEastAsian(const FontVariantEastAsian east_asian) {
  Set(PropertySetFlag::kVariantEastAsian);

  font_description_.SetVariantEastAsian(east_asian);
}

void FontBuilder::SetVariantLigatures(
    const FontDescription::VariantLigatures& ligatures) {
  Set(PropertySetFlag::kVariantLigatures);

  font_description_.SetVariantLigatures(ligatures);
}

void FontBuilder::SetVariantNumeric(const FontVariantNumeric& variant_numeric) {
  Set(PropertySetFlag::kVariantNumeric);

  font_description_.SetVariantNumeric(variant_numeric);
}

void FontBuilder::SetFontSynthesisWeight(
    FontDescription::FontSynthesisWeight font_synthesis_weight) {
  Set(PropertySetFlag::kFontSynthesisWeight);

  font_description_.SetFontSynthesisWeight(font_synthesis_weight);
}

void FontBuilder::SetFontSynthesisStyle(
    FontDescription::FontSynthesisStyle font_synthesis_style) {
  Set(PropertySetFlag::kFontSynthesisStyle);

  font_description_.SetFontSynthesisStyle(font_synthesis_style);
}

void FontBuilder::SetFontSynthesisSmallCaps(
    FontDescription::FontSynthesisSmallCaps font_synthesis_small_caps) {
  Set(PropertySetFlag::kFontSynthesisSmallCaps);

  font_description_.SetFontSynthesisSmallCaps(font_synthesis_small_caps);
}

void FontBuilder::SetTextRendering(TextRenderingMode text_rendering_mode) {
  Set(PropertySetFlag::kTextRendering);

  font_description_.SetTextRendering(text_rendering_mode);
}

void FontBuilder::SetKerning(FontDescription::Kerning kerning) {
  Set(PropertySetFlag::kKerning);

  font_description_.SetKerning(kerning);
}

void FontBuilder::SetTextSpacingTrim(TextSpacingTrim text_spacing_trim) {
  Set(PropertySetFlag::kTextSpacingTrim);
  font_description_.SetTextSpacingTrim(text_spacing_trim);
}

void FontBuilder::SetFontOpticalSizing(OpticalSizing font_optical_sizing) {
  Set(PropertySetFlag::kFontOpticalSizing);

  font_description_.SetFontOpticalSizing(font_optical_sizing);
}

void FontBuilder::SetFontPalette(scoped_refptr<const FontPalette> palette) {
  Set(PropertySetFlag::kFontPalette);
  font_description_.SetFontPalette(std::move(palette));
}

void FontBuilder::SetFontVariantAlternates(
    scoped_refptr<const FontVariantAlternates> variant_alternates) {
  Set(PropertySetFlag::kFontVariantAlternates);
  font_description_.SetFontVariantAlternates(std::move(variant_alternates));
}

void FontBuilder::SetFontSmoothing(FontSmoothingMode font_smoothing_mode) {
  Set(PropertySetFlag::kFontSmoothing);
  font_description_.SetFontSmoothing(font_smoothing_mode);
}

void FontBuilder::SetFeatureSettings(
    scoped_refptr<const FontFeatureSettings> settings) {
  Set(PropertySetFlag::kFeatureSettings);
  font_description_.SetFeatureSettings(std::move(settings));
}

void FontBuilder::SetVariationSettings(
    scoped_refptr<const FontVariationSettings> settings) {
  Set(PropertySetFlag::kVariationSettings);
  font_description_.SetVariationSettings(std::move(settings));
}

void FontBuilder::SetFamilyDescription(
    FontDescription& font_description,
    const FontDescription::FamilyDescription& family_description) {
  Set(PropertySetFlag::kFamily);

  bool is_initial =
      family_description.generic_family == FontDescription::kStandardFamily &&
      family_description.family.FamilyName().empty();

  font_description.SetGenericFamily(family_description.generic_family);
  font_description.SetFamily(is_initial ? StandardFontFamily()
                                        : family_description.family);
}

void FontBuilder::SetSize(FontDescription& font_description,
                          const FontDescription::Size& size) {
  float specified_size = size.value;

  if (specified_size < 0) {
    return;
  }

  Set(PropertySetFlag::kSize);

  // Overly large font sizes will cause crashes on some platforms (such as
  // Windows).  Cap font size here to make sure that doesn't happen.
  specified_size = std::min(kMaximumAllowedFontSize, specified_size);

  font_description.SetKeywordSize(size.keyword);
  font_description.SetSpecifiedSize(specified_size);
  font_description.SetIsAbsoluteSize(size.is_absolute);
}

void FontBuilder::SetVariantPosition(
    FontDescription::FontVariantPosition variant_position) {
  Set(PropertySetFlag::kVariantPosition);

  font_description_.SetVariantPosition(variant_position);
}

void FontBuilder::SetVariantEmoji(FontVariantEmoji variant_emoji) {
  Set(PropertySetFlag::kVariantEmoji);

  font_description_.SetVariantEmoji(variant_emoji);
}

float FontBuilder::GetComputedSizeFromSpecifiedSize(
    const FontDescription& font_description,
    const ComputedStyleBuilder& builder,
    float specified_size) {
  DCHECK(document_);
  float zoom_factor = builder.EffectiveZoom();
  // Apply the text zoom factor preference. The preference is exposed in
  // accessibility settings in Chrome for Android to improve readability.
  if (LocalFrame* frame = document_->GetFrame()) {
    zoom_factor *= frame->TextZoomFactor();
  }

  if (!builder.GetTextSizeAdjust().IsAuto()) {
    if (RuntimeEnabledFeatures::TextSizeAdjustImprovementsEnabled()) {
      Settings* settings = document_->GetSettings();
      if (settings && settings->GetTextAutosizingEnabled()) {
        zoom_factor *= builder.GetTextSizeAdjust().Multiplier();
      }
    }
  }

  return FontSizeFunctions::GetComputedSizeFromSpecifiedSize(
      document_, zoom_factor, font_description.IsAbsoluteSize(),
      specified_size);
}

void FontBuilder::CheckForGenericFamilyChange(
    const FontDescription& parent_description,
    FontDescription& new_description) {
  DCHECK(document_);
  if (new_description.IsAbsoluteSize()) {
    return;
  }

  if (new_description.IsMonospace() == parent_description.IsMonospace()) {
    return;
  }

  // We know the parent is monospace or the child is monospace, and that font
  // size was unspecified. We want to scale our font size as appropriate.
  // If the font uses a keyword size, then we refetch from the table rather than
  // multiplying by our scale factor.
  float size;
  if (new_description.KeywordSize()) {
    size = FontSizeForKeyword(new_description.KeywordSize(),
                              new_description.IsMonospace());
  } else {
    Settings* settings = document_->GetSettings();
    float fixed_scale_factor =
        (settings && settings->GetDefaultFixedFontSize() &&
         settings->GetDefaultFontSize())
            ? static_cast<float>(settings->GetDefaultFixedFontSize()) /
                  settings->GetDefaultFontSize()
            : 1;
    size = parent_description.IsMonospace()
               ? new_description.SpecifiedSize() / fixed_scale_factor
               : new_description.SpecifiedSize() * fixed_scale_factor;
  }

  new_description.SetSpecifiedSize(size);
}

void FontBuilder::UpdateSpecifiedSize(
    FontDescription& font_description,
    const FontDescription& parent_description) {
  float specified_size = font_description.SpecifiedSize();

  if (!specified_size && font_description.KeywordSize()) {
    specified_size = FontSizeForKeyword(font_description.KeywordSize(),
                                        font_description.IsMonospace());
  }
  font_description.SetSpecifiedSize(specified_size);

  CheckForGenericFamilyChange(parent_description, font_description);
}

void FontBuilder::UpdateAdjustedSize(FontDescription& font_description,
                                     FontSelector* font_selector) {
  // Note: the computed_size has scale/zooming applied as well as text auto-
  // sizing and Android font scaling. That means we operate on the used value
  // without font-size-adjust applied and apply the font-size-adjust to end up
  // at a new adjusted_size.
  const float computed_size = font_description.ComputedSize();
  if (!font_description.HasSizeAdjust() || !computed_size) {
    return;
  }

  // We need to create a temporal Font to get xHeight of a primary font.
  // The aspect value is based on the xHeight of the font for the computed font
  // size, so we need to reset the adjusted_size to computed_size. See
  // FontDescription::EffectiveFontSize.
  font_description.SetAdjustedSize(computed_size);

  Font font(font_description, font_selector);

  const SimpleFontData* font_data = font.PrimaryFont();
  if (!font_data) {
    return;
  }

  FontSizeAdjust size_adjust = font_description.SizeAdjust();
  if (size_adjust.IsFromFont() &&
      size_adjust.Value() == FontSizeAdjust::kFontSizeAdjustNone) {
    std::optional<float> aspect_value = FontSizeFunctions::FontAspectValue(
        font_data, size_adjust.GetMetric(), font_description.ComputedSize());
    font_description.SetSizeAdjust(FontSizeAdjust(
        aspect_value.has_value() ? aspect_value.value()
                                 : FontSizeAdjust::kFontSizeAdjustNone,
        size_adjust.GetMetric(), FontSizeAdjust::ValueType::kFromFont));
  }

  if (auto adjusted_size = FontSizeFunctions::MetricsMultiplierAdjustedFontSize(
          font_data, font_description)) {
    font_description.SetAdjustedSize(adjusted_size.value());
  }
}

void FontBuilder::UpdateComputedSize(FontDescription& font_description,
                                     const ComputedStyleBuilder& builder) {
  float computed_size = GetComputedSizeFromSpecifiedSize(
      font_description, builder, font_description.SpecifiedSize());
  computed_size = TextAutosizer::ComputeAutosizedFontSize(
      computed_size, builder.TextAutosizingMultiplier(),
      builder.EffectiveZoom());
  font_description.SetComputedSize(computed_size);
}

bool FontBuilder::UpdateFontDescription(FontDescription& description,
                                        FontOrientation font_orientation) {
  bool modified = false;
  if (IsSet(PropertySetFlag::kFamily)) {
    if (description.GenericFamily() != font_description_.GenericFamily() ||
        description.Family() != font_description_.Family()) {
      modified = true;
      description.SetGenericFamily(font_description_.GenericFamily());
      description.SetFamily(font_description_.Family());
    }
  }
  if (IsSet(PropertySetFlag::kSize)) {
    if (description.KeywordSize() != font_description_.KeywordSize() ||
        description.SpecifiedSize() != font_description_.SpecifiedSize() ||
        description.IsAbsoluteSize() != font_description_.IsAbsoluteSize()) {
      modified = true;
      description.SetKeywordSize(font_description_.KeywordSize());
      description.SetSpecifiedSize(font_description_.SpecifiedSize());
      description.SetIsAbsoluteSize(font_description_.IsAbsoluteSize());
    }
  }

  if (IsSet(PropertySetFlag::kSizeAdjust)) {
    if (description.SizeAdjust() != font_description_.SizeAdjust()) {
      modified = true;
      description.SetSizeAdjust(font_description_.SizeAdjust());
    }
  }
  if (IsSet(PropertySetFlag::kWeight)) {
    if (description.Weight() != font_description_.Weight()) {
      modified = true;
      description.SetWeight(font_description_.Weight());
    }
  }
  if (IsSet(PropertySetFlag::kStretch)) {
    if (description.Stretch() != font_description_.Stretch()) {
      modified = true;
      description.SetStretch(font_description_.Stretch());
    }
  }
  if (IsSet(PropertySetFlag::kFeatureSettings)) {
    if (description.FeatureSettings() != font_description_.FeatureSettings()) {
      modified = true;
      description.SetFeatureSettings(font_description_.FeatureSettings());
    }
  }
  if (IsSet(PropertySetFlag::kLocale)) {
    if (description.Locale() != font_description_.Locale()) {
      modified = true;
      description.SetLocale(font_description_.Locale());
    }
  }
  if (IsSet(PropertySetFlag::kStyle)) {
    if (description.Style() != font_description_.Style()) {
      modified = true;
      description.SetStyle(font_description_.Style());
    }
  }
  if (IsSet(PropertySetFlag::kVariantCaps)) {
    if (description.VariantCaps() != font_description_.VariantCaps()) {
      modified = true;
      description.SetVariantCaps(font_description_.VariantCaps());
    }
  }
  if (IsSet(PropertySetFlag::kVariantEastAsian)) {
    if (description.VariantEastAsian() !=
        font_description_.VariantEastAsian()) {
      modified = true;
      description.SetVariantEastAsian(font_description_.VariantEastAsian());
    }
  }
  if (IsSet(PropertySetFlag::kVariantLigatures)) {
    if (description.GetVariantLigatures() !=
        font_description_.GetVariantLigatures()) {
      modified = true;
      description.SetVariantLigatures(font_description_.GetVariantLigatures());
    }
  }
  if (IsSet(PropertySetFlag::kVariantNumeric)) {
    if (description.VariantNumeric() != font_description_.VariantNumeric()) {
      modified = true;
      description.SetVariantNumeric(font_description_.VariantNumeric());
    }
  }
  if (IsSet(PropertySetFlag::kVariationSettings)) {
    if (description.VariationSettings() !=
        font_description_.VariationSettings()) {
      modified = true;
      description.SetVariationSettings(font_description_.VariationSettings());
    }
  }
  if (IsSet(PropertySetFlag::kFontSynthesisWeight)) {
    if (description.GetFontSynthesisWeight() !=
        font_description_.GetFontSynthesisWeight()) {
      modified = true;
      description.SetFontSynthesisWeight(
          font_description_.GetFontSynthesisWeight());
    }
  }
  if (IsSet(PropertySetFlag::kFontSynthesisStyle)) {
    if (description.GetFontSynthesisStyle() !=
        font_description_.GetFontSynthesisStyle()) {
      modified = true;
      description.SetFontSynthesisStyle(
          font_description_.GetFontSynthesisStyle());
    }
  }
  if (IsSet(PropertySetFlag::kFontSynthesisSmallCaps)) {
    if (description.GetFontSynthesisSmallCaps() !=
        font_description_.GetFontSynthesisSmallCaps()) {
      modified = true;
      description.SetFontSynthesisSmallCaps(
          font_description_.GetFontSynthesisSmallCaps());
    }
  }
  if (IsSet(PropertySetFlag::kTextRendering)) {
    if (description.TextRendering() != font_description_.TextRendering()) {
      modified = true;
      description.SetTextRendering(font_description_.TextRendering());
    }
  }
  if (IsSet(PropertySetFlag::kKerning)) {
    if (description.GetKerning() != font_description_.GetKerning()) {
      modified = true;
      description.SetKerning(font_description_.GetKerning());
    }
  }
  if (IsSet(PropertySetFlag::kTextSpacingTrim)) {
    if (description.GetTextSpacingTrim() !=
        font_description_.GetTextSpacingTrim()) {
      modified = true;
      description.SetTextSpacingTrim(font_description_.GetTextSpacingTrim());
    }
  }
  if (IsSet(PropertySetFlag::kFontOpticalSizing)) {
    if (description.FontOpticalSizing() !=
        font_description_.FontOpticalSizing()) {
      modified = true;
      description.SetFontOpticalSizing(font_description_.FontOpticalSizing());
    }
  }
  if (IsSet(PropertySetFlag::kFontPalette)) {
    if (description.GetFontPalette() != font_description_.GetFontPalette()) {
      modified = true;
      description.SetFontPalette(font_description_.GetFontPalette());
    }
  }
  if (IsSet(PropertySetFlag::kFontVariantAlternates)) {
    if (description.GetFontVariantAlternates() !=
        font_description_.GetFontVariantAlternates()) {
      modified = true;
      description.SetFontVariantAlternates(
          font_description_.GetFontVariantAlternates());
    }
  }
  if (IsSet(PropertySetFlag::kFontSmoothing)) {
    if (description.FontSmoothing() != font_description_.FontSmoothing()) {
      modified = true;
      description.SetFontSmoothing(font_description_.FontSmoothing());
    }
  }
  if (IsSet(PropertySetFlag::kTextOrientation) ||
      IsSet(PropertySetFlag::kWritingMode)) {
    if (description.Orientation() != font_orientation) {
      modified = true;
      description.SetOrientation(font_orientation);
    }
  }
  if (IsSet(PropertySetFlag::kVariantPosition)) {
    if (description.VariantPosition() != font_description_.VariantPosition()) {
      modified = true;
      description.SetVariantPosition(font_description_.VariantPosition());
    }
  }
  if (IsSet(PropertySetFlag::kVariantEmoji)) {
    if (description.VariantEmoji() != font_description_.VariantEmoji()) {
      modified = true;
      description.SetVariantEmoji(font_description_.VariantEmoji());
    }
  }
  if (!modified && !IsSet(PropertySetFlag::kEffectiveZoom) &&
      !IsSet(PropertySetFlag::kTextSizeAdjust)) {
    return false;
  }

  float size = description.SpecifiedSize();
  if (!size && description.KeywordSize()) {
    size = FontSizeForKeyword(description.KeywordSize(),
                              description.IsMonospace());
  }

  description.SetSpecifiedSize(size);
  description.SetComputedSize(size);
  if (size && description.HasSizeAdjust()) {
    description.SetAdjustedSize(size);
  }
  return true;
}

FontSelector* FontBuilder::FontSelectorFromTreeScope(
    const TreeScope* tree_scope) {
  // TODO(crbug.com/437837): The tree_scope may be from a different Document in
  // the case where we are resolving style for elements in a <svg:use> shadow
  // tree.
  DCHECK(!tree_scope || tree_scope->GetDocument() == document_ ||
         tree_scope->GetDocument().IsSVGDocument());
  // TODO(crbug.com/336876): Font selector should be based on tree_scope for
  // tree-scoped references.
  return document_->GetStyleEngine().GetFontSelector();
}

FontSelector* FontBuilder::ComputeFontSelector(
    const ComputedStyleBuilder& builder) {
  if (IsSet(PropertySetFlag::kFamily)) {
    return FontSelectorFromTreeScope(family_tree_scope_);
  } else {
    return builder.GetFont().GetFontSelector();
  }
}

void FontBuilder::CreateFont(ComputedStyleBuilder& builder,
                             const ComputedStyle* parent_style) {
  DCHECK(document_);

  if (!flags_) {
    return;
  }

  // TODO(crbug.com/1086680): Avoid nullptr parent style.
  const FontDescription& parent_description =
      parent_style ? parent_style->GetFontDescription()
                   : builder.GetFontDescription();

  FontDescription description = builder.GetFontDescription();
  if (!UpdateFontDescription(description, builder.ComputeFontOrientation())) {
    // Early exit; nothing was actually changed (i.e., everything that was set
    // already matched the initial/parent style).
    flags_ = 0;
    return;
  }
  UpdateSpecifiedSize(description, parent_description);
  UpdateComputedSize(description, builder);

  FontSelector* font_selector = ComputeFontSelector(builder);
  UpdateAdjustedSize(description, font_selector);

  builder.SetFont(Font(description, font_selector));
  flags_ = 0;
}

void FontBuilder::CreateInitialFont(ComputedStyleBuilder& builder) {
  DCHECK(document_);
  FontDescription font_description = FontDescription();
  font_description.SetLocale(builder.GetFontDescription().Locale());

  SetFamilyDescription(font_description,
                       FontBuilder::InitialFamilyDescription());
  SetSize(font_description,
          FontDescription::Size(FontSizeFunctions::InitialKeywordSize(), 0.0f,
                                false));
  UpdateSpecifiedSize(font_description, builder.GetFontDescription());
  UpdateComputedSize(font_description, builder);

  font_description.SetOrientation(builder.ComputeFontOrientation());

  FontSelector* font_selector = document_->GetStyleEngine().GetFontSelector();
  builder.SetFont(Font(font_description, font_selector));
}

}  // namespace blink
