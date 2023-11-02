// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_TEXT_CONTROL_INNER_EDITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_TEXT_CONTROL_INNER_EDITOR_H_

#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"

namespace blink {

// LayoutNGTextControlInnerEditor is a LayoutObject for 'InnerEditor' elements
// in <input> and <textarea>.
class LayoutNGTextControlInnerEditor final : public LayoutNGBlockFlow {
 public:
  explicit LayoutNGTextControlInnerEditor(Element* element)
      : LayoutNGBlockFlow(element) {}

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGTextControlInnerEditor";
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_TEXT_CONTROL_INNER_EDITOR_H_
