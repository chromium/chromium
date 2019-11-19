/*
 * Copyright (C) 2007 Apple Computer, Inc.
 * Copyright (c) 2007, 2008, 2009, Google Inc. All rights reserved.
 * Copyright (C) 2010 Company 100, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/opentype/font_format_check.h"
#include "third_party/blink/renderer/platform/fonts/opentype/font_settings.h"
#include "third_party/blink/renderer/platform/fonts/web_font_decoder.h"
#include "third_party/blink/renderer/platform/fonts/web_font_typeface_factory.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

namespace {
sk_sp<SkFontMgr> FontManagerForSubType(
    FontFormatCheck::VariableFontSubType font_sub_type) {
  CHECK_NE(font_sub_type, FontFormatCheck::VariableFontSubType::kNotVariable);
  if (font_sub_type == FontFormatCheck::VariableFontSubType::kVariableCFF2)
    return WebFontTypefaceFactory::FreeTypeFontManager();
  return WebFontTypefaceFactory::FontManagerForVariations();
}
}  // namespace

FontCustomPlatformData::FontCustomPlatformData(sk_sp<SkTypeface> typeface,
                                               size_t data_size)
    : base_typeface_(std::move(typeface)), data_size_(data_size) {}

FontCustomPlatformData::~FontCustomPlatformData() = default;

FontPlatformData FontCustomPlatformData::GetFontPlatformData(
    float size,
    bool bold,
    bool italic,
    const FontSelectionRequest& selection_request,
    const FontSelectionCapabilities& selection_capabilities,
    const OpticalSizing& optical_sizing,
    FontOrientation orientation,
    const FontVariationSettings* variation_settings) {
  DCHECK(base_typeface_);

  sk_sp<SkTypeface> return_typeface = base_typeface_;

  // Maximum axis count is maximum value for the OpenType USHORT,
  // which is a 16bit unsigned.
  // https://www.microsoft.com/typography/otspec/fvar.htm Variation
  // settings coming from CSS can have duplicate assignments and the
  // list can be longer than UINT16_MAX, but ignoring the length for
  // now, going with a reasonable upper limit. Deduplication is
  // handled by Skia with priority given to the last occuring
  // assignment.
  FontFormatCheck::VariableFontSubType font_sub_type =
      FontFormatCheck::ProbeVariableFont(base_typeface_);
  if (font_sub_type ==
          FontFormatCheck::VariableFontSubType::kVariableTrueType ||
      font_sub_type == FontFormatCheck::VariableFontSubType::kVariableCFF2) {
    Vector<SkFontArguments::Axis, 0> axes;

    SkFontArguments::Axis weight_axis = {
        SkSetFourByteTag('w', 'g', 'h', 't'),
        SkFloatToScalar(selection_capabilities.weight.clampToRange(
            selection_request.weight))};
    SkFontArguments::Axis width_axis = {
        SkSetFourByteTag('w', 'd', 't', 'h'),
        SkFloatToScalar(selection_capabilities.width.clampToRange(
            selection_request.width))};
    SkFontArguments::Axis slant_axis = {
        SkSetFourByteTag('s', 'l', 'n', 't'),
        SkFloatToScalar(selection_capabilities.slope.clampToRange(
            selection_request.slope))};

    axes.push_back(weight_axis);
    axes.push_back(width_axis);
    axes.push_back(slant_axis);

    bool explicit_opsz_configured = false;
    if (variation_settings && variation_settings->size() < UINT16_MAX) {
      axes.ReserveCapacity(variation_settings->size() + axes.size());
      for (const auto& setting : *variation_settings) {
        if (setting.Tag() == AtomicString("opsz"))
          explicit_opsz_configured = true;
        SkFontArguments::Axis axis = {AtomicStringToFourByteTag(setting.Tag()),
                                      SkFloatToScalar(setting.Value())};
        axes.push_back(axis);
      }
    }

    if (optical_sizing == kAutoOpticalSizing && !explicit_opsz_configured) {
      SkFontArguments::Axis opsz_axis = {SkSetFourByteTag('o', 'p', 's', 'z'),
                                         SkFloatToScalar(size)};
      axes.push_back(opsz_axis);
    }

    int index;
    std::unique_ptr<SkStreamAsset> stream(base_typeface_->openStream(&index));
    sk_sp<SkTypeface> sk_variation_font(FontManagerForSubType(font_sub_type)
        ->makeFromStream(std::move(stream),
                         SkFontArguments().setCollectionIndex(index)
                                          .setAxes(axes.data(), axes.size())));

    if (sk_variation_font) {
      return_typeface = sk_variation_font;
    } else {
      SkString family_name;
      base_typeface_->getFamilyName(&family_name);
      // TODO: Surface this as a console message?
      LOG(ERROR) << "Unable for apply variation axis properties for font: "
                 << family_name.c_str();
    }
  }

  return FontPlatformData(std::move(return_typeface), std::string(), size,
                          bold && !base_typeface_->isBold(),
                          italic && !base_typeface_->isItalic(), orientation);
}

String FontCustomPlatformData::FamilyNameForInspector() const {
  SkTypeface::LocalizedStrings* font_family_iterator =
      base_typeface_->createFamilyNameIterator();
  SkTypeface::LocalizedString localized_string;
  while (font_family_iterator->next(&localized_string)) {
    // BCP 47 tags for English take precedent in font matching over other
    // localizations: https://drafts.csswg.org/css-fonts/#descdef-src.
    if (localized_string.fLanguage.equals("en") ||
        localized_string.fLanguage.equals("en-US")) {
      break;
    }
  }
  font_family_iterator->unref();
  return String::FromUTF8(localized_string.fString.c_str(),
                          localized_string.fString.size());
}

scoped_refptr<FontCustomPlatformData> FontCustomPlatformData::Create(
    SharedBuffer* buffer,
    String& ots_parse_message) {
  DCHECK(buffer);
  WebFontDecoder decoder;
  sk_sp<SkTypeface> typeface = decoder.Decode(buffer);
  if (!typeface) {
    ots_parse_message = decoder.GetErrorString();
    return nullptr;
  }
  return base::AdoptRef(
      new FontCustomPlatformData(std::move(typeface), decoder.DecodedSize()));
}

bool FontCustomPlatformData::SupportsFormat(const String& format) {
  // Support relevant format specifiers from
  // https://drafts.csswg.org/css-fonts-4/#src-desc
  return EqualIgnoringASCIICase(format, "woff") ||
         EqualIgnoringASCIICase(format, "truetype") ||
         EqualIgnoringASCIICase(format, "opentype") ||
         EqualIgnoringASCIICase(format, "woff2") ||
         EqualIgnoringASCIICase(format, "woff-variations") ||
         EqualIgnoringASCIICase(format, "truetype-variations") ||
         EqualIgnoringASCIICase(format, "opentype-variations") ||
         EqualIgnoringASCIICase(format, "woff2-variations");
}

}  // namespace blink
