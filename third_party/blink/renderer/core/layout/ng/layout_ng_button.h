// Copyright 2020 The Chromium Authors. All rights reserved.
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

  const char* GetName() const override { return "LayoutNGButton"; }
  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;
  void RemoveChild(LayoutObject*) override;
  void RemoveLeftoverAnonymousBlock(LayoutBlock*) override {}
  bool CreatesAnonymousWrapper() const override { return true; }

 private:
  void UpdateAnonymousChildStyle(const LayoutObject* child,
                                 ComputedStyle& child_style) const override;

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectNGButton || LayoutNGFlexibleBox::IsOfType(type);
  }

  LayoutBlock* inner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_BUTTON_H_
