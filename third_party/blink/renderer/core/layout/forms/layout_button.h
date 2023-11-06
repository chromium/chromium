// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FORMS_LAYOUT_BUTTON_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FORMS_LAYOUT_BUTTON_H_

#include "third_party/blink/renderer/core/layout/flex/layout_flexible_box.h"

namespace blink {

class LayoutButton final : public LayoutFlexibleBox {
 public:
  explicit LayoutButton(Element*);
  ~LayoutButton() override;
  void Trace(Visitor*) const override;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutButton";
  }
  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;
  void RemoveChild(LayoutObject*) override;
  void RemoveLeftoverAnonymousBlock(LayoutBlock*) override { NOT_DESTROYED(); }

  static bool ShouldCountWrongBaseline(const LayoutBox& button_box,
                                       const ComputedStyle& style,
                                       const ComputedStyle* parent_style);

 private:
  void UpdateAnonymousChildStyle(
      const LayoutObject* child,
      ComputedStyleBuilder& child_style_builder) const override;

  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectButton || LayoutFlexibleBox::IsOfType(type);
  }

  Member<LayoutBlock> inner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FORMS_LAYOUT_BUTTON_H_
