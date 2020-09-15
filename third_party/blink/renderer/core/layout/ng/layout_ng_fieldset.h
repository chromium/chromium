// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_FIELDSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_FIELDSET_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"

namespace blink {

class CORE_EXPORT LayoutNGFieldset final : public LayoutNGBlockFlow {
 public:
  explicit LayoutNGFieldset(Element*);

  const char* GetName() const override { return "LayoutNGFieldset"; }

  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;

  bool CreatesNewFormattingContext() const final { return true; }

 protected:
  bool IsOfType(LayoutObjectType) const override;
  void UpdateAnonymousChildStyle(const LayoutObject* child,
                                 ComputedStyle& child_style) const override;
  void InvalidatePaint(const PaintInvalidatorContext& context) const final;
  bool BackgroundIsKnownToBeOpaqueInRect(const PhysicalRect&) const override;
  bool HitTestChildren(HitTestResult& result,
                       const HitTestLocation& hit_test_location,
                       const PhysicalOffset& accumulated_offset,
                       HitTestAction hit_test_action) override;

  bool AllowsNonVisibleOverflow() const override { return false; }
  // Override to forward to the anonymous fieldset content box.
  LayoutUnit ScrollWidth() const override;
  LayoutUnit ScrollHeight() const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_FIELDSET_H_
