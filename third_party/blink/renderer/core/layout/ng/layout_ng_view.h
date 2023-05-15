// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_VIEW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_VIEW_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow_mixin.h"

namespace blink {

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGBlockFlowMixin<LayoutView>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT LayoutNGMixin<LayoutView>;

class CORE_EXPORT LayoutNGView : public LayoutNGBlockFlowMixin<LayoutView> {
 public:
  explicit LayoutNGView(ContainerNode*);
  ~LayoutNGView() override;

  bool IsFragmentationContextRoot() const override;

  void UpdateBlockLayout() override;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGView";
  }

 protected:
  bool IsOfType(LayoutObjectType) const override;

 private:
  MinMaxSizes ComputeIntrinsicLogicalWidths() const override;
  AtomicString NamedPageAtIndex(wtf_size_t page_index) const override;
};

template <>
struct DowncastTraits<LayoutNGView> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutNGView();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_VIEW_H_
