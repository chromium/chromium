// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPositionedFloat_h
#define NGPositionedFloat_h

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_offset.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"

namespace blink {

class NGLayoutResult;

// Contains the information necessary for copying back data to a FloatingObject.
struct CORE_EXPORT NGPositionedFloat {
  NGPositionedFloat(scoped_refptr<NGLayoutResult> layout_result,
                    const NGBfcOffset& bfc_offset)
      : layout_result(layout_result), bfc_offset(bfc_offset) {}
  NGPositionedFloat(NGPositionedFloat&&) noexcept = default;
  NGPositionedFloat(const NGPositionedFloat&) = default;
  NGPositionedFloat& operator=(NGPositionedFloat&&) = default;
  NGPositionedFloat& operator=(const NGPositionedFloat&) = default;

  scoped_refptr<NGLayoutResult> layout_result;
  NGBfcOffset bfc_offset;
};

}  // namespace blink

#endif  // NGPositionedFloat_h
