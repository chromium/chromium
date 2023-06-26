// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_PARAGRAPH_LINE_BREAKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_PARAGRAPH_LINE_BREAKER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class NGConstraintSpace;
class NGInlineNode;
struct NGLineLayoutOpportunity;

class CORE_EXPORT NGParagraphLineBreaker {
 public:
  static absl::optional<LayoutUnit> AttemptParagraphBalancing(
      const NGInlineNode& node,
      const NGConstraintSpace& space,
      const NGLineLayoutOpportunity& line_opportunity);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_PARAGRAPH_LINE_BREAKER_H_
