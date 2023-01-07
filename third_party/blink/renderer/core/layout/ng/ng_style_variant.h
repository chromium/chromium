// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_STYLE_VARIANT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_STYLE_VARIANT_H_

#include "base/compiler_specific.h"

namespace blink {

// LayoutObject can have multiple style variations.
enum class NGStyleVariant { kStandard, kFirstLine, kEllipsis };

inline NGStyleVariant ToParentStyleVariant(NGStyleVariant variant) {
  // Ancestors of ellipsis should not use the ellipsis style variant. Use
  // first-line style if exists since most cases it is the first line. See also
  // |LayoutObject::SlowEffectiveStyle|.
  if (UNLIKELY(variant == NGStyleVariant::kEllipsis))
    return NGStyleVariant::kFirstLine;
  return variant;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_STYLE_VARIANT_H_
