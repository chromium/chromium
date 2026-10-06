// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/text_fragment_paint_info.h"

namespace blink {

TextFragmentPaintInfo TextFragmentPaintInfo::Slice(wtf_size_t slice_from,
                                                   wtf_size_t slice_to) const {
  DCHECK_LE(from, slice_from);
  DCHECK_LE(slice_from, slice_to);
  DCHECK_LE(slice_to, to);
  TextFragmentPaintInfo result = *this;
  result.from = slice_from;
  result.to = slice_to;
  return result;
}

TextFragmentPaintInfo TextFragmentPaintInfo::WithStartOffset(
    wtf_size_t start_from) const {
  return Slice(start_from, to);
}

TextFragmentPaintInfo TextFragmentPaintInfo::WithEndOffset(
    wtf_size_t end_to) const {
  return Slice(from, end_to);
}

}  // namespace blink
