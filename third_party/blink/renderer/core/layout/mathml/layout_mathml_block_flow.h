// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MATHML_LAYOUT_MATHML_BLOCK_FLOW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MATHML_LAYOUT_MATHML_BLOCK_FLOW_H_

#include "third_party/blink/renderer/core/layout/layout_ng_block_flow.h"

namespace blink {

class Element;

class LayoutMathMLBlockFlow final : public LayoutNGBlockFlow {
 public:
  explicit LayoutMathMLBlockFlow(Element*);

  const char* GetName() const final {
    NOT_DESTROYED();
    return "LayoutMathMLBlockFlow";
  }

 private:
  bool IsOfType(LayoutObjectType) const final;
  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const final {
    NOT_DESTROYED();
    return true;
  }
  bool CreatesNewFormattingContext() const final {
    NOT_DESTROYED();
    return true;
  }

  bool IsMonolithic() const final {
    NOT_DESTROYED();
    return true;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MATHML_LAYOUT_MATHML_BLOCK_FLOW_H_
