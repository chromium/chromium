// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_LAYOUT_NG_MATHML_BLOCK_WITH_ANONYMOUS_MROW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_LAYOUT_NG_MATHML_BLOCK_WITH_ANONYMOUS_MROW_H_

#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_mathml_block.h"

namespace blink {

class LayoutNGMathMLBlockWithAnonymousMrow : public LayoutNGMathMLBlock {
 public:
  explicit LayoutNGMathMLBlockWithAnonymousMrow(Element*);

  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_LAYOUT_NG_MATHML_BLOCK_WITH_ANONYMOUS_MROW_H_
