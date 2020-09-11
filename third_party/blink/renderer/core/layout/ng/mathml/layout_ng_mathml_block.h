// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_LAYOUT_NG_MATHML_BLOCK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_LAYOUT_NG_MATHML_BLOCK_H_

#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"

namespace blink {

class LayoutNGMathMLBlock : public LayoutNGMixin<LayoutBlock> {
 public:
  explicit LayoutNGMathMLBlock(Element*);

  const char* GetName() const override { return "LayoutNGMathMLBlock"; }

 private:
  void UpdateBlockLayout(bool relayout_children) final;

  bool IsOfType(LayoutObjectType) const final;
  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const final;
  bool CanHaveChildren() const final;
  void StyleDidChange(StyleDifference, const ComputedStyle*) final;

  PaginationBreakability GetPaginationBreakability() const final {
    return kForbidBreaks;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_LAYOUT_NG_MATHML_BLOCK_H_
