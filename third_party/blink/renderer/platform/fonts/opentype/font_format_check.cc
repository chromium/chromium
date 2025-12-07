// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/font_format_check.h"

#include "base/containers/span.h"
#include "base/containers/span_rust.h"
#include "base/numerics/byte_conversions.h"
#include "third_party/blink/renderer/platform/fonts/opentype/format_check.rs.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

FontFormatCheck::FontFormatCheck(sk_sp<SkData> sk_data)
    : format_info_(font_format_check::get_font_format_info(
          base::SpanToRustSlice(sk_data->byteSpan()))) {}

bool FontFormatCheck::IsVariableFont() const {
  return font_format_check::is_variable(*format_info_);
}

bool FontFormatCheck::IsCbdtCblcColorFont() const {
  return font_format_check::is_cbdt_cblc(*format_info_);
}

bool FontFormatCheck::IsColrCpalColorFontV0() const {
  return font_format_check::is_colrv0(*format_info_);
}

bool FontFormatCheck::IsColrCpalColorFontV1() const {
  return font_format_check::is_colrv1(*format_info_);
}

bool FontFormatCheck::IsSbixColorFont() const {
  return font_format_check::is_sbix(*format_info_);
}

bool FontFormatCheck::IsCff2OutlineFont() const {
  return font_format_check::is_cff2(*format_info_);
}

bool FontFormatCheck::IsVariableColrV0Font() const {
  return IsColrCpalColorFontV0() && IsVariableFont();
}

bool FontFormatCheck::IsColorFont() const {
  return IsSbixColorFont() || IsCbdtCblcColorFont() ||
         IsColrCpalColorFontV0() || IsColrCpalColorFontV1();
}

FontFormatCheck::VariableFontSubType FontFormatCheck::ProbeVariableFont(
    sk_sp<SkTypeface> typeface) {
  if (!typeface->getTableSize(
          SkFontTableTag(SkSetFourByteTag('f', 'v', 'a', 'r'))))
    return VariableFontSubType::kNotVariable;

  if (typeface->getTableSize(
          SkFontTableTag(SkSetFourByteTag('C', 'F', 'F', '2'))))
    return VariableFontSubType::kVariableCFF2;
  return VariableFontSubType::kVariableTrueType;
}

}  // namespace blink
