// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATHML_PAINT_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATHML_PAINT_INFO_H_

#include <unicode/uchar.h>
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

class ShapeResultView;

struct CORE_EXPORT NGMathMLPaintInfo {
  USING_FAST_MALLOC(NGMathMLPaintInfo);

 public:
  UChar operator_character;
  scoped_refptr<const ShapeResultView> operator_shape_result_view;
  LayoutUnit operator_inline_size;
  LayoutUnit operator_ascent;
  LayoutUnit operator_descent;
  NGBoxStrut radical_base_margins;
  LayoutUnit radical_operator_inline_offset;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATHML_PAINT_INFO_H_
