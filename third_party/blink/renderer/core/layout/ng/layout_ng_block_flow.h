// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_BLOCK_FLOW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_BLOCK_FLOW_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow_mixin.h"

namespace blink {

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGBlockFlowMixin<LayoutBlockFlow>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGMixin<LayoutBlockFlow>;

// This overrides the default layout block algorithm to use Layout NG.
class CORE_EXPORT LayoutNGBlockFlow
    : public LayoutNGBlockFlowMixin<LayoutBlockFlow> {
 public:
  explicit LayoutNGBlockFlow(ContainerNode*);
  ~LayoutNGBlockFlow() override;

  void UpdateBlockLayout(bool relayout_children) override;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGBlockFlow";
  }

 protected:
  bool IsOfType(LayoutObjectType) const override;
};

template <>
struct DowncastTraits<LayoutNGBlockFlow> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutNGBlockFlow();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_BLOCK_FLOW_H_
