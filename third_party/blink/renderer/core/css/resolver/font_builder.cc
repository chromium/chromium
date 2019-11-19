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

#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"

namespace blink {

FontBuilder::FontBuilder(const Document* document)
    : document_(document), flags_(0) {
  DCHECK(!document || document->GetFrame());
}

void FontBuilder::SetInitial(float effective_zoom) {
  DCHECK(document_->GetSettings());
  if (!document_->GetSettings())
    return;

  SetFamilyDescription(font_description_,
                       FontBuilder::InitialFamilyDescription());
  SetSize(font_description_, FontBuilder::InitialSize());
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

FontFamily FontBuilder::StandardFontFamily() const {
  FontFamily family;
  family.SetFamily(StandardFontFamilyName());
  return family;
}

AtomicString FontBuilder::StandardFontFamilyName() const {
  Settings* settings = document_->GetSettings();
  if (settings)
    return settings->GetGenericFontFamilySettings().Standard();
  return AtomicString();
}

AtomicString FontBuilder::GenericFontFamilyName(
    FontDescription::GenericFamilyType generic_family) const {
  switch (generic_family) {
    default:
      NOTREACHED();
      FALLTHROUGH;
    case FontDescription::kNoFamily:
      return AtomicString();
    case FontDescription::kStandardFamily:
      return StandardFontFamilyName();
    case FontDescription::kSerifFamily:
      return font_family_names::kWebkitSerif;
    case FontDescription::kSansSerifFamily:
      return font_family_names::kWebkitSansSerif;
    case FontDescription::kMonospaceFamily:
      return font_family_names::kWebkitMonospace;
    case FontDescription::kCursiveFamily:
      return font_family_names::kWebkitCursive;
    case FontDescription::kFantasyFamily:
      return font_family_names::kWebkitFantasy;
    case FontDescription::kPictographFamily:
      return font_family_names::kWebkitPictograph;
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

void FontBuilder::SetSizeAdjust(float aspect_value) {
  Set(PropertySetFlag::kSizeAdjust);

  font_description_.SetSizeAdjust(aspect_value);
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

void FontBuilder::SetTextRendering(TextRenderingMode text_rendering_mode) {
  Set(PropertySetFlag::kTextRendering);

  font_description_.SetTextRendering(text_rendering_mode);
}

void FontBuilder::SetKerning(FontDescription::Kerning kerning) {
  Set(PropertySetFlag::kKerning);

  font_description_.SetKerning(kerning);
}

void FontBuilder::SetFontOpticalSizing(OpticalSizing font_optical_sizing) {
  Set(PropertySetFlag::kFontOpticalSizing);

  font_description_.SetFontOpticalSizing(font_optical_sizing);
}

void FontBuilder::SetFontSmoothing(FontSmoothingMode foont_smoothing_mode) {
  Set(PropertySetFlag::kFontSmoothing);

  font_description_.SetFontSmoothing(foont_smoothing_mode);
}

void FontBuilder::SetFeatureSettings(
    scoped_refptr<FontFeatureSettings> settings) {
  Set(PropertySetFlag::kFeatureSettings);
  font_description_.SetFeatureSettings(std::move(settings));
}

void FontBuilder::SetVariationSettings(
    scoped_refptr<FontVariationSettings> settings) {
  Set(PropertySetFlag::kVariationSettings);
  font_description_.SetVariationSettings(std::move(settings));
}

void FontBuilder::SetFamilyDescription(
    FontDescription& font_description,
    const FontDescription::FamilyDescription& family_description) {
  Set(PropertySetFlag::kFamily);

  bool is_initial =
      family_description.generic_family == FontDescription::kStandardFamily &&
      family_description.family.FamilyIsEmpty();

  font_description.SetGenericFamily(family_description.generic_family);
  font_description.SetFamily(is_initial ? StandardFontFamily()
                                        : family_description.family);
}

void FontBuilder::SetSize(FontDescription& font_description,
                          const FontDescription::Size& size) {
  float specified_size = size.value;

  if (specified_size < 0)
    return;

  Set(PropertySetFlag::kSize);

  // Overly large font sizes will cause crashes on some platforms (such as
  // Windows).  Cap font size here to make sure that doesn't happen.
  specified_size = std::min(kMaximumAllowedFontSize, specified_size);

  font_description.SetKeywordSize(size.keyword);
  font_description.SetSpecifiedSize(specified_size);
  font_description.SetIsAbsoluteSize(size.is_absolute);
}

float FontBuilder::GetComputedSizeFromSpecifiedSize(
    FontDescription& font_description,
    float effective_zoom,
    float specified_size) {
  DCHECK(document_);
  float zoom_factor = effective_zoom;
  // Apply the text zoom factor preference. The preference is exposed in
  // accessibility settings in Chrome for Android to improve readability.
  if (LocalFrame* frame = document_->GetFrame())
    zoom_factor *= frame->TextZoomFactor();

  return FontSizeFunctions::GetComputedSizeFromSpecifiedSize(
      document_, zoom_factor, font_description.IsAbsoluteSize(),
      specified_size);
}

void FontBuilder::CheckForGenericFamilyChange(
    const FontDescription& old_description,
    FontDescription& new_description) {
  DCHECK(document_);
  if (new_description.IsAbsoluteSize())
    return;

  if (new_description.IsMonospace() == old_description.IsMonospace())
    return;

  // For now, lump all families but monospace together.
  if (new_description.GenericFamily() != FontDescription::kMonospaceFamily &&
      old_description.GenericFamily() != FontDescription::kMonospaceFamily)
    return;

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
    size = old_description.IsMonospace()
               ? new_description.SpecifiedSize() / fixed_scale_factor
               : new_description.SpecifiedSize() * fixed_scale_factor;
  }

  new_description.SetSpecifiedSize(size);
}

void FontBuilder::UpdateSpecifiedSize(FontDescription& font_description,
                                      const ComputedStyle& style) {
  float specified_size = font_description.SpecifiedSize();

  if (!specified_size && font_description.KeywordSize())
    specified_size = FontSizeForKeyword(font_description.KeywordSize(),
                                        font_description.IsMonospace());

  font_description.SetSpecifiedSize(specified_size);

  CheckForGenericFamilyChange(style.GetFontDescription(), font_description);
}

void FontBuilder::UpdateAdjustedSize(FontDescription& font_description,
                                     const ComputedStyle& style,
                                     FontSelector* font_selector) {
  const float specified_size = font_description.SpecifiedSize();
  if (!font_description.HasSizeAdjust() || !specified_size)
    return;

  // We need to create a temporal Font to get xHeight of a primary font.
  // The aspect value is based on the xHeight of the font for the computed font
  // size, so we need to reset the adjustedSize to computedSize. See
  // FontDescription::EffectiveFontSize.
  font_description.SetAdjustedSize(font_description.ComputedSize());

  Font font(font_description);
  font.Update(font_selector);

  const SimpleFontData* font_data = font.PrimaryFont();

  if (!font_data || !font_data->GetFontMetrics().HasXHeight())
    return;

  const float size_adjust = font_description.SizeAdjust();
  float aspect_value = font_data->GetFontMetrics().XHeight() / specified_size;
  float adjusted_size = (size_adjust / aspect_value) * specified_size;
  adjusted_size = GetComputedSizeFromSpecifiedSize(
      font_description, style.EffectiveZoom(), adjusted_size);

  adjusted_size = TextAutosizer::ComputeAutosizedFontSize(
      adjusted_size, style.TextAutosizingMultiplier(), style.EffectiveZoom());
  font_description.SetAdjustedSize(adjusted_size);
}

void FontBuilder::UpdateComputedSize(FontDescription& font_description,
                                     const ComputedStyle& style) {
  float computed_size =
      GetComputedSizeFromSpecifiedSize(font_description, style.EffectiveZoom(),
                                       font_description.SpecifiedSize());
  computed_size = TextAutosizer::ComputeAutosizedFontSize(
      computed_size, style.TextAutosizingMultiplier(), style.EffectiveZoom());
  font_description.SetComputedSize(computed_size);
}

void FontBuilder::UpdateFontDescription(FontDescription& description,
                                        FontOrientation font_orientation) {
  if (IsSet(PropertySetFlag::kFamily)) {
    description.SetGenericFamily(font_description_.GenericFamily());
    description.SetFamily(font_description_.Family());
  }
  if (IsSet(PropertySetFlag::kSize)) {
    description.SetKeywordSize(font_description_.KeywordSize());
    description.SetSpecifiedSize(font_description_.SpecifiedSize());
    description.SetIsAbsoluteSize(font_description_.IsAbsoluteSize());
  }

  if (IsSet(PropertySetFlag::kSizeAdjust))
    description.SetSizeAdjust(font_description_.SizeAdjust());
  if (IsSet(PropertySetFlag::kWeight))
    description.SetWeight(font_description_.Weight());
  if (IsSet(PropertySetFlag::kStretch))
    description.SetStretch(font_description_.Stretch());
  if (IsSet(PropertySetFlag::kFeatureSettings))
    description.SetFeatureSettings(font_description_.FeatureSettings());
  if (IsSet(PropertySetFlag::kLocale))
    description.SetLocale(font_description_.Locale());
  if (IsSet(PropertySetFlag::kStyle))
    description.SetStyle(font_description_.Style());
  if (IsSet(PropertySetFlag::kVariantCaps))
    description.SetVariantCaps(font_description_.VariantCaps());
  if (IsSet(PropertySetFlag::kVariantEastAsian))
    description.SetVariantEastAsian(font_description_.VariantEastAsian());
  if (IsSet(PropertySetFlag::kVariantLigatures))
    description.SetVariantLigatures(font_description_.GetVariantLigatures());
  if (IsSet(PropertySetFlag::kVariantNumeric))
    description.SetVariantNumeric(font_description_.VariantNumeric());
  if (IsSet(PropertySetFlag::kVariationSettings))
    description.SetVariationSettings(font_description_.VariationSettings());
  if (IsSet(PropertySetFlag::kTextRendering))
    description.SetTextRendering(font_description_.TextRendering());
  if (IsSet(PropertySetFlag::kKerning))
    description.SetKerning(font_description_.GetKerning());
  if (IsSet(PropertySetFlag::kFontOpticalSizing))
    description.SetFontOpticalSizing(font_description_.FontOpticalSizing());
  if (IsSet(PropertySetFlag::kFontSmoothing))
    description.SetFontSmoothing(font_description_.FontSmoothing());
  if (IsSet(PropertySetFlag::kTextOrientation) ||
      IsSet(PropertySetFlag::kWritingMode))
    description.SetOrientation(font_orientation);

  float size = description.SpecifiedSize();
  if (!size && description.KeywordSize()) {
    size = FontSizeForKeyword(description.KeywordSize(),
                              description.IsMonospace());
  }

  description.SetSpecifiedSize(size);
  description.SetComputedSize(size);
  if (size && description.HasSizeAdjust())
    description.SetAdjustedSize(size);
}

void FontBuilder::CreateFont(FontSelector* font_selector,
                             ComputedStyle& style) {
  DCHECK(document_);

  if (!flags_)
    return;

  FontDescription description = style.GetFontDescription();

  UpdateFontDescription(description, style.ComputeFontOrientation());

  UpdateSpecifiedSize(description, style);
  UpdateComputedSize(description, style);
  UpdateAdjustedSize(description, style, font_selector);

  style.SetFontDescription(description);
  style.GetFont().Update(font_selector);
  flags_ = 0;
}

void FontBuilder::CreateFontForDocument(FontSelector* font_selector,
                                        ComputedStyle& document_style) {
  DCHECK(document_);
  FontDescription font_description = FontDescription();
  font_description.SetLocale(document_style.GetFontDescription().Locale());

  SetFamilyDescription(font_description,
                       FontBuilder::InitialFamilyDescription());
  SetSize(font_description,
          FontDescription::Size(FontSizeFunctions::InitialKeywordSize(), 0.0f,
                                false));
  UpdateSpecifiedSize(font_description, document_style);
  UpdateComputedSize(font_description, document_style);

  font_description.SetOrientation(document_style.ComputeFontOrientation());
  document_style.SetFontDescription(font_description);
  document_style.GetFont().Update(font_selector);
}

}  // namespace blink
