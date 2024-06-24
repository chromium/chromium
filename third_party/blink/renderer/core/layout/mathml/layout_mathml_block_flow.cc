// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/mathml/layout_mathml_block_flow.h"

namespace blink {

LayoutMathMLBlockFlow::LayoutMathMLBlockFlow(Element* element)
    : LayoutBlockFlow(element) {
  DCHECK(element);
}

}  // namespace blink
