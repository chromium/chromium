// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_PROGRESS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_PROGRESS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_progress.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow_mixin.h"

namespace blink {

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGBlockFlowMixin<LayoutProgress>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT LayoutNGMixin<LayoutProgress>;

class CORE_EXPORT LayoutNGProgress
    : public LayoutNGBlockFlowMixin<LayoutProgress> {
 public:
  explicit LayoutNGProgress(Element*);
  ~LayoutNGProgress() override;

  void UpdateBlockLayout(bool relayout_children) override;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGProgress";
  }

 protected:
  bool IsOfType(LayoutObjectType type) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_PROGRESS_H_
