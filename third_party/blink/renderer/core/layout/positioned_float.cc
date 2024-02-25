// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/positioned_float.h"

#include "third_party/blink/renderer/core/layout/block_break_token.h"

namespace blink {

void PositionedFloat::Trace(Visitor* visitor) const {
  visitor->Trace(layout_result);
  visitor->Trace(break_before_token);
}

const BlockBreakToken* PositionedFloat::BreakToken() const {
  if (break_before_token) {
    return break_before_token.Get();
  }
  return To<BlockBreakToken>(
      layout_result->GetPhysicalFragment().GetBreakToken());
}

}  // namespace blink
