// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/font_format_check.h"

// Include HarfBuzz to have a cross-platform way to retrieve table tags without
// having to rely on the platform being able to instantiate this font format.
#include <hb.h>

#include <hb-cplusplus.hh>

#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

namespace {

FontFormatCheck::COLRVersion determineCOLRVersion(
    const FontFormatCheck::TableTagsVector& table_tags,
    const hb_face_t* face) {
  const hb_tag_t kCOLRTag = HB_TAG('C', 'O', 'L', 'R');

  // Only try to read version if header size is sufficient.
  // https://docs.microsoft.com/en-us/typography/opentype/spec/colr#header
  const unsigned int kMinCOLRHeaderSize = 14;
  if (table_tags.size() && table_tags.Contains(kCOLRTag) &&
      table_tags.Contains(HB_TAG('C', 'P', 'A', 'L'))) {
    hb::unique_ptr<hb_blob_t> table_blob(
        hb_face_reference_table(face, kCOLRTag));
    if (hb_blob_get_length(table_blob.get()) < kMinCOLRHeaderSize)
      return FontFormatCheck::COLRVersion::kNoCOLR;

    unsigned required_bytes_count = 2u;
    const char* colr_ptr =
        hb_blob_get_data(table_blob.get(), &required_bytes_count);
    base::span<const uint8_t> colr_data = base::as_bytes(
        // SAFETY: hb_blob_get_data() populates the 2nd argument with the
        // number of bytes at the returned pointer.
        UNSAFE_BUFFERS(base::span(colr_ptr, required_bytes_count)));

    if (colr_data.size() < 2u) {
      return FontFormatCheck::COLRVersion::kNoCOLR;
    }
    uint16_t colr_version = base::U16FromBigEndian(colr_data.first<2u>());

    if (colr_version == 0)
      return FontFormatCheck::COLRVersion::kCOLRV0;
    else if (colr_version == 1)
      return FontFormatCheck::COLRVersion::kCOLRV1;
  }
  return FontFormatCheck::COLRVersion::kNoCOLR;
}

}  // namespace

FontFormatCheck::FontFormatCheck(sk_sp<SkData> sk_data) {
  hb::unique_ptr<hb_blob_t> font_blob(
      hb_blob_create(reinterpret_cast<const char*>(sk_data->bytes()),
                     base::checked_cast<unsigned>(sk_data->size()),
                     HB_MEMORY_MODE_READONLY, nullptr, nullptr));
  hb::unique_ptr<hb_face_t> face(hb_face_create(font_blob.get(), 0));

  unsigned table_count = 0;
  table_count = hb_face_get_table_tags(face.get(), 0, nullptr, nullptr);
  table_tags_.resize(table_count);
  if (!hb_face_get_table_tags(face.get(), 0, &table_count, table_tags_.data()))
    table_tags_.resize(0);

  colr_version_ = determineCOLRVersion(table_tags_, face.get());
}

bool FontFormatCheck::IsVariableFont() const {
  return table_tags_.size() && table_tags_.Contains(HB_TAG('f', 'v', 'a', 'r'));
}

bool FontFormatCheck::IsCbdtCblcColorFont() const {
  return table_tags_.size() &&
         table_tags_.Contains(HB_TAG('C', 'B', 'D', 'T')) &&
         table_tags_.Contains(HB_TAG('C', 'B', 'L', 'C'));
}

bool FontFormatCheck::IsColrCpalColorFontV0() const {
  return colr_version_ == COLRVersion::kCOLRV0;
}

bool FontFormatCheck::IsColrCpalColorFontV1() const {
  return colr_version_ == COLRVersion::kCOLRV1;
}

bool FontFormatCheck::IsVariableColrV0Font() const {
  return IsColrCpalColorFontV0() && IsVariableFont();
}

bool FontFormatCheck::IsSbixColorFont() const {
  return table_tags_.size() && table_tags_.Contains(HB_TAG('s', 'b', 'i', 'x'));
}

bool FontFormatCheck::IsCff2OutlineFont() const {
  return table_tags_.size() && table_tags_.Contains(HB_TAG('C', 'F', 'F', '2'));
}

bool FontFormatCheck::IsColorFont() const {
  return IsCbdtCblcColorFont() || IsColrCpalColorFont() || IsSbixColorFont();
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
