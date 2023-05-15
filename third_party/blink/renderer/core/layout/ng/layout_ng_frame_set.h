// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_FRAME_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_FRAME_SET_H_

#include "third_party/blink/renderer/core/layout/ng/layout_ng_block.h"

namespace blink {

class LayoutNGFrameSet final : public LayoutNGBlock {
 public:
  explicit LayoutNGFrameSet(Element*);

 private:
  const char* GetName() const override;
  bool IsOfType(LayoutObjectType type) const override;
  bool IsChildAllowed(LayoutObject* child, const ComputedStyle&) const override;
  void AddChild(LayoutObject* new_child, LayoutObject* before_child) override;
  void RemoveChild(LayoutObject* child) override;
  void UpdateBlockLayout() override;
  CursorDirective GetCursor(const PhysicalOffset& point,
                            ui::Cursor& cursor) const override;
};

template <>
struct DowncastTraits<LayoutNGFrameSet> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsFrameSet();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_FRAME_SET_H_
