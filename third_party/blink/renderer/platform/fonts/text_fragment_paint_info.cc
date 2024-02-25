// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/text_fragment_paint_info.h"

namespace blink {

TextFragmentPaintInfo TextFragmentPaintInfo::Slice(unsigned slice_from,
                                                   unsigned slice_to) const {
  DCHECK_LE(from, slice_from);
  DCHECK_LE(slice_from, slice_to);
  DCHECK_LE(slice_to, to);
  return {text, slice_from, slice_to, shape_result};
}

TextFragmentPaintInfo TextFragmentPaintInfo::WithStartOffset(
    unsigned start_from) const {
  return Slice(start_from, to);
}

TextFragmentPaintInfo TextFragmentPaintInfo::WithEndOffset(
    unsigned end_to) const {
  return Slice(from, end_to);
}

}  // namespace blink
