// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_POSITIONED_FLOAT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_POSITIONED_FLOAT_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_offset.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"

namespace blink {

class NGLayoutResult;

// Contains the information necessary for copying back data to a FloatingObject.
struct CORE_EXPORT NGPositionedFloat {
  NGPositionedFloat(scoped_refptr<const NGLayoutResult> layout_result,
                    const NGBfcOffset& bfc_offset,
                    bool need_break_before = false)
      : layout_result(layout_result),
        bfc_offset(bfc_offset),
        need_break_before(need_break_before) {}
  NGPositionedFloat(NGPositionedFloat&&) noexcept = default;
  NGPositionedFloat(const NGPositionedFloat&) = default;
  NGPositionedFloat& operator=(NGPositionedFloat&&) = default;
  NGPositionedFloat& operator=(const NGPositionedFloat&) = default;

  scoped_refptr<const NGLayoutResult> layout_result;
  NGBfcOffset bfc_offset;
  bool need_break_before = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_POSITIONED_FLOAT_H_
