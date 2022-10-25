// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_BUTTON_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_BUTTON_H_

#include "third_party/blink/renderer/core/layout/ng/flex/layout_ng_flexible_box.h"

namespace blink {

class LayoutNGButton final : public LayoutNGFlexibleBox {
 public:
  explicit LayoutNGButton(Element*);
  ~LayoutNGButton() override;
  void Trace(Visitor*) const override;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGButton";
  }
  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;
  void RemoveChild(LayoutObject*) override;
  void RemoveLeftoverAnonymousBlock(LayoutBlock*) override { NOT_DESTROYED(); }
  bool CreatesAnonymousWrapper() const override {
    NOT_DESTROYED();
    return true;
  }

 private:
  void UpdateAnonymousChildStyle(
      const LayoutObject* child,
      ComputedStyleBuilder& child_style_builder) const override;

  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectNGButton || LayoutNGFlexibleBox::IsOfType(type);
  }

  Member<LayoutBlock> inner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_BUTTON_H_
