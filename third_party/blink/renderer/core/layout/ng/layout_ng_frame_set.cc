// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_frame_set.h"

#include "third_party/blink/renderer/core/html/html_frame_set_element.h"

namespace blink {

LayoutNGFrameSet::LayoutNGFrameSet(Element* element) : LayoutNGBlock(element) {
  DCHECK(IsA<HTMLFrameSetElement>(element));
}

const char* LayoutNGFrameSet::GetName() const {
  NOT_DESTROYED();
  return "LayoutNGFrameSet";
}

bool LayoutNGFrameSet::IsOfType(LayoutObjectType type) const {
  NOT_DESTROYED();
  return type == kLayoutObjectNGFrameSet || LayoutNGBlock::IsOfType(type);
}

}  // namespace blink
