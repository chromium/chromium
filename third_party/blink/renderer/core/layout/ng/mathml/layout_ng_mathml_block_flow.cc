// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_mathml_block_flow.h"

namespace blink {

LayoutNGMathMLBlockFlow::LayoutNGMathMLBlockFlow(Element* element)
    : LayoutNGBlockFlow(element) {
  DCHECK(element);
}

bool LayoutNGMathMLBlockFlow::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectMathML || LayoutNGBlockFlow::IsOfType(type);
}

}  // namespace blink
