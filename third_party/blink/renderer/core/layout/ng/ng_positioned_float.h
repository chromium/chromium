// Copyright 2017 The Chromium Authors
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
  DISALLOW_NEW();

 public:
  NGPositionedFloat() = default;
  NGPositionedFloat(const NGLayoutResult* layout_result,
                    const NGBlockBreakToken* break_before_token,
                    const NGBfcOffset& bfc_offset,
                    LayoutUnit minimum_space_shortage)
      : layout_result(layout_result),
        break_before_token(break_before_token),
        bfc_offset(bfc_offset),
        minimum_space_shortage(minimum_space_shortage) {}
  NGPositionedFloat(NGPositionedFloat&&) noexcept = default;
  NGPositionedFloat(const NGPositionedFloat&) = default;
  NGPositionedFloat& operator=(NGPositionedFloat&&) = default;
  NGPositionedFloat& operator=(const NGPositionedFloat&) = default;

  void Trace(Visitor*) const;

  const NGBlockBreakToken* BreakToken() const;

  Member<const NGLayoutResult> layout_result;
  Member<const NGBlockBreakToken> break_before_token;
  NGBfcOffset bfc_offset;
  LayoutUnit minimum_space_shortage;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::NGPositionedFloat)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_POSITIONED_FLOAT_H_
