// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ng_flat_tree_shorthands.h"

#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_position.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_rect.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_utils.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"

namespace blink {

const LayoutBlockFlow* NGInlineFormattingContextOf(
    const PositionInFlatTree& position) {
  return NGInlineFormattingContextOf(ToPositionInDOMTree(position));
}

NGCaretPosition ComputeNGCaretPosition(
    const PositionInFlatTreeWithAffinity& position) {
  return ComputeNGCaretPosition(ToPositionInDOMTreeWithAffinity(position));
}

bool InSameNGLineBox(const PositionInFlatTreeWithAffinity& position1,
                     const PositionInFlatTreeWithAffinity& position2) {
  return InSameNGLineBox(ToPositionInDOMTreeWithAffinity(position1),
                         ToPositionInDOMTreeWithAffinity(position2));
}

}  // namespace blink
