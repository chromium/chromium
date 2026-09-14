// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FORMS_LAYOUT_TEXT_CONTROL_MULTI_LINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FORMS_LAYOUT_TEXT_CONTROL_MULTI_LINE_H_

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

namespace blink {

// LayoutTextControlMultiLine is a LayoutObject for <textarea>.
class LayoutTextControlMultiLine final : public LayoutBlockFlow {
 public:
  explicit LayoutTextControlMultiLine(Element* element);

  // The ellipsis is not always rendered by the scroller itself: a textarea
  // scrolls on its host but keeps each line in an anonymous block inside the
  // inner editor. Those blocks would reuse their cached layout and stay
  // truncated.
  void SetNeedsLayoutForTextOverflowChange();

 private:
  HTMLElement* InnerEditorElement() const;

  bool IsTextArea() const final {
    NOT_DESTROYED();
    return true;
  }

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutTextControlMultiLine";
  }

  bool CreatesNewFormattingContext() const override {
    NOT_DESTROYED();
    return true;
  }

  void StyleDidChange(StyleDifference,
                      const ComputedStyle* old_style,
                      const ComputedStyle& new_style,
                      const StyleChangeContext&) override;

  bool NodeAtPoint(HitTestResult& result,
                   const HitTestLocation& hit_test_location,
                   const PhysicalOffset& accumulated_offset,
                   HitTestPhase phase) override;
};

template <>
struct DowncastTraits<LayoutTextControlMultiLine> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsTextArea();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FORMS_LAYOUT_TEXT_CONTROL_MULTI_LINE_H_
