// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_TEXT_CONTROL_SINGLE_LINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_TEXT_CONTROL_SINGLE_LINE_H_

#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"

namespace blink {

// LayoutNGTextControlSingleLine is a LayoutObject for textfield <input>.
class LayoutNGTextControlSingleLine final : public LayoutNGBlockFlow {
 public:
  explicit LayoutNGTextControlSingleLine(Element* element);

 private:
  HTMLElement* InnerEditorElement() const;
  Element* ContainerElement() const;
  Element* EditingViewPortElement() const;

  bool IsOfType(LayoutObjectType) const override;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGTextControlSingleLine";
  }

  bool CreatesNewFormattingContext() const override {
    NOT_DESTROYED();
    return true;
  }

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  bool NodeAtPoint(HitTestResult& result,
                   const HitTestLocation& hit_test_location,
                   const PhysicalOffset& accumulated_offset,
                   HitTestPhase phase) override;

  bool RespectsCSSOverflow() const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_TEXT_CONTROL_SINGLE_LINE_H_
