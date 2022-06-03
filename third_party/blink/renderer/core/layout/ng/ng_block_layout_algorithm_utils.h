// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_LAYOUT_ALGORITHM_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_LAYOUT_ALGORITHM_UTILS_H_

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class ComputedStyle;
struct NGBfcOffset;
class NGExclusionSpace;

// OOF-positioned nodes which were initially inline-level, however are in a
// block-level context, pretend they are in an inline-level context. E.g.
// they avoid floats, and respect text-align.
//
// This function calculates the inline-offset to avoid floats, and respect
// text-align.
//
// TODO(ikilpatrick): Move this back into ng_block_layout_algorithm.cc
LayoutUnit CalculateOutOfFlowStaticInlineLevelOffset(
    const ComputedStyle& container_style,
    const NGBfcOffset& origin_bfc_offset,
    const NGExclusionSpace&,
    LayoutUnit child_available_inline_size);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_LAYOUT_ALGORITHM_UTILS_H_
