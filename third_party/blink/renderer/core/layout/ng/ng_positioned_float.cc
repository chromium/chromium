// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"

namespace blink {

void NGPositionedFloat::Trace(Visitor* visitor) const {
  visitor->Trace(layout_result);
  visitor->Trace(break_before_token);
}

const NGBlockBreakToken* NGPositionedFloat::BreakToken() const {
  if (break_before_token) {
    return break_before_token.Get();
  }
  return To<NGBlockBreakToken>(layout_result->PhysicalFragment().BreakToken());
}

}  // namespace blink
