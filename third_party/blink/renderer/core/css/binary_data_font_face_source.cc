// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/binary_data_font_face_source.h"

#include "third_party/blink/renderer/core/css/css_font_face.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

BinaryDataFontFaceSource::BinaryDataFontFaceSource(CSSFontFace* css_font_face,
                                                   SharedBuffer* data,
                                                   String& ots_parse_message)
    : custom_platform_data_(
          FontCustomPlatformData::Create(data, ots_parse_message)) {
  if (!css_font_face || !css_font_face->GetFontFace()) {
    return;
  }
  FontFace* font_face = css_font_face->GetFontFace();
  ExecutionContext* context = font_face->GetExecutionContext();
  if (!context) {
    return;
  }
  probe::FontsUpdated(context, font_face, String(),
                      custom_platform_data_.Get());
}

void BinaryDataFontFaceSource::Trace(Visitor* visitor) const {
  visitor->Trace(custom_platform_data_);
  CSSFontFaceSource::Trace(visitor);
}

bool BinaryDataFontFaceSource::IsValid() const {
  return custom_platform_data_;
}

SimpleFontData* BinaryDataFontFaceSource::CreateFontData(
    const FontDescription& font_description,
    const FontSelectionCapabilities& font_selection_capabilities) {
  return MakeGarbageCollected<SimpleFontData>(
      custom_platform_data_->GetFontPlatformData(
          font_description.EffectiveFontSize(),
          font_description.AdjustedSpecifiedSize(),
          font_description.IsSyntheticBold() &&
              font_description.SyntheticBoldAllowed(),
          font_description.IsSyntheticItalic() &&
              font_description.SyntheticItalicAllowed(),
          font_description.GetFontSelectionRequest(),
          font_selection_capabilities, font_description.FontOpticalSizing(),
          font_description.TextRendering(),
          font_description.GetFontVariantAlternates()
              ? font_description.GetFontVariantAlternates()
                    ->GetResolvedFontFeatures()
              : ResolvedFontFeatures(),
          font_description.Orientation(), font_description.VariationSettings(),
          font_description.GetFontPalette()),
      MakeGarbageCollected<CustomFontData>());
}

}  // namespace blink
