// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_LAYOUT_NG_SVG_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_LAYOUT_NG_SVG_TEXT_H_

#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow_mixin.h"

namespace blink {

// The LayoutNG representation of SVG <text>.
class LayoutNGSVGText final : public LayoutNGBlockFlowMixin<LayoutSVGBlock> {
 public:
  explicit LayoutNGSVGText(Element* element);

  void SubtreeStructureChanged(LayoutInvalidationReasonForTracing);
  void SetNeedsTextMetricsUpdate() {
    NOT_DESTROYED();
    needs_text_metrics_update_ = true;
  }

 private:
  // LayoutObject override:
  const char* GetName() const override;
  bool IsOfType(LayoutObjectType type) const override;
  bool IsChildAllowed(LayoutObject* child, const ComputedStyle&) const override;
  void AddChild(LayoutObject* child, LayoutObject* before_child) override;
  void RemoveChild(LayoutObject* child) override;

  // LayoutBox override:
  bool CreatesNewFormattingContext() const override;

  // LayoutBlock override:
  void UpdateBlockLayout(bool relayout_children) override;

  void UpdateFont();

  bool needs_text_metrics_update_ : 1;
};

template <>
struct DowncastTraits<LayoutNGSVGText> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsNGSVGText();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_LAYOUT_NG_SVG_TEXT_H_
