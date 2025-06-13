// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/color_table_lookup.h"

#include "base/containers/heap_array.h"

namespace blink {

namespace {
SkFontTableTag kCpalTag = SkSetFourByteTag('C', 'P', 'A', 'L');
SkFontTableTag kColrTag = SkSetFourByteTag('C', 'O', 'L', 'R');
SkFontTableTag kSbixTag = SkSetFourByteTag('s', 'b', 'i', 'x');
SkFontTableTag kCbdtTag = SkSetFourByteTag('C', 'B', 'D', 'T');
SkFontTableTag kCblcTag = SkSetFourByteTag('C', 'B', 'L', 'C');
}  // namespace

bool ColorTableLookup::TypefaceHasAnySupportedColorTable(
    const SkTypeface* typeface) {
  if (!typeface) {
    return false;
  }
  const int num_tags = typeface->countTables();
  if (!num_tags) {
    return false;
  }
  auto tags = base::HeapArray<SkFontTableTag>::Uninit(num_tags);
  const int returned_tags = typeface->readTableTags(tags);
  if (!returned_tags) {
    return false;
  }
  bool has_cpal = false;
  bool has_colr = false;
  bool has_cbdt = false;
  bool has_cblc = false;
  for (int i = 0; i < returned_tags; i++) {
    SkFontTableTag tag = tags[i];
    if (tag == kSbixTag) {
      return true;
    }
    if (tag == kCpalTag) {
      if (has_colr) {
        return true;
      }
      has_cpal = true;
    } else if (tag == kColrTag) {
      if (has_cpal) {
        return true;
      }
      has_colr = true;
    } else if (tag == kCbdtTag) {
      if (has_cblc) {
        return true;
      }
      has_cbdt = true;
    } else if (tag == kCblcTag) {
      if (has_cbdt) {
        return true;
      }
      has_cblc = true;
    }
  }
  return false;
}

}  // namespace blink
