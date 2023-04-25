// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_LAYOUT_NG_MATHML_BLOCK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_LAYOUT_NG_MATHML_BLOCK_H_

#include "third_party/blink/renderer/core/layout/ng/layout_ng_block.h"

namespace blink {

class LayoutNGMathMLBlock : public LayoutNGBlock {
 public:
  explicit LayoutNGMathMLBlock(Element*);

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGMathMLBlock";
  }

 private:
  void UpdateBlockLayout() final;

  bool IsOfType(LayoutObjectType) const final;
  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const final;
  bool CanHaveChildren() const final;
  void StyleDidChange(StyleDifference, const ComputedStyle*) final;

  bool IsMonolithic() const final {
    NOT_DESTROYED();
    return true;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_LAYOUT_NG_MATHML_BLOCK_H_
