// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_LAYOUT_NG_MATHML_BLOCK_FLOW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_LAYOUT_NG_MATHML_BLOCK_FLOW_H_

#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"

namespace blink {

class Element;

class LayoutNGMathMLBlockFlow final : public LayoutNGBlockFlow {
 public:
  explicit LayoutNGMathMLBlockFlow(Element*);

  const char* GetName() const final { return "LayoutNGMathMLBlockFlow"; }

 private:
  bool IsOfType(LayoutObjectType) const final;
  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const final {
    return true;
  }
  bool CreatesNewFormattingContext() const final { return true; }

  PaginationBreakability GetPaginationBreakability() const final {
    return kForbidBreaks;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_LAYOUT_NG_MATHML_BLOCK_FLOW_H_
