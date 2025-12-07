// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FORMS_LAYOUT_TEXT_CONTROL_INNER_EDITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FORMS_LAYOUT_TEXT_CONTROL_INNER_EDITOR_H_

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

namespace blink {

// LayoutTextControlInnerEditor is a LayoutObject for 'InnerEditor' elements
// in <input> and <textarea>.
class LayoutTextControlInnerEditor final : public LayoutBlockFlow {
 public:
  explicit LayoutTextControlInnerEditor(Element* element);

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutTextControlInnerEditor";
  }

  bool IsTextControlInnerEditor() const override {
    NOT_DESTROYED();
    return true;
  }

  // Returns true if the host is a <textarea> and TextareaMultipleIfcs flag
  // is enabled.
  bool IsMultiline() const {
    NOT_DESTROYED();
    return is_multiline_;
  }

  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;
  void StyleDidChange(StyleDifference diff,
                      const ComputedStyle* old_style,
                      const StyleChangeContext&) override;

 private:
  const bool is_multiline_;
};

template <>
struct DowncastTraits<LayoutTextControlInnerEditor> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsTextControlInnerEditor();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FORMS_LAYOUT_TEXT_CONTROL_INNER_EDITOR_H_
