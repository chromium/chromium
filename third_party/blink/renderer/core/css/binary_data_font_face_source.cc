// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/binary_data_font_face_source.h"

#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

BinaryDataFontFaceSource::BinaryDataFontFaceSource(SharedBuffer* data,
                                                   String& ots_parse_message)
    : custom_platform_data_(
          FontCustomPlatformData::Create(data, ots_parse_message)) {}

BinaryDataFontFaceSource::~BinaryDataFontFaceSource() = default;

bool BinaryDataFontFaceSource::IsValid() const {
  return custom_platform_data_.get();
}

scoped_refptr<SimpleFontData> BinaryDataFontFaceSource::CreateFontData(
    const FontDescription& font_description,
    const FontSelectionCapabilities& font_selection_capabilities) {
  return SimpleFontData::Create(
      custom_platform_data_->GetFontPlatformData(
          font_description.EffectiveFontSize(),
          font_description.IsSyntheticBold(),
          font_description.IsSyntheticItalic(),
          font_description.GetFontSelectionRequest(),
          font_selection_capabilities, font_description.FontOpticalSizing(),
          font_description.Orientation(), font_description.VariationSettings()),
      CustomFontData::Create());
}

}  // namespace blink
