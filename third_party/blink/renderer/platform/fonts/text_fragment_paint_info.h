// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_TEXT_FRAGMENT_PAINT_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_TEXT_FRAGMENT_PAINT_INFO_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

class ShapeResultView;

// Bridge struct for painting text. Encapsulates info needed by the paint code.
struct PLATFORM_EXPORT TextFragmentPaintInfo {
  STACK_ALLOCATED();

 public:
  TextFragmentPaintInfo Slice(wtf_size_t slice_from, wtf_size_t slice_to) const;
  TextFragmentPaintInfo WithStartOffset(wtf_size_t start_from) const;
  TextFragmentPaintInfo WithEndOffset(wtf_size_t end_to) const;
  wtf_size_t Length() const { return to - from; }

  // The string to paint. May include surrounding context.
  const StringView text;

  // The range of the |text| to paint.
  wtf_size_t from;
  wtf_size_t to;

  // The |shape_result| may not contain all characters of the |text|, but is
  // guaranteed to contain |from| to |to|.
  const ShapeResultView* shape_result;

  // Paint-time scaling factor for `text-fit`. It's 1.0 for SVG text.
  float text_fit_scaling_factor = 1.0f;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_TEXT_FRAGMENT_PAINT_INFO_H_
