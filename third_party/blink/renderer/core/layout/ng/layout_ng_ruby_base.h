// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_RUBY_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_RUBY_BASE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_base.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow_mixin.h"

namespace blink {

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGBlockFlowMixin<LayoutRubyBase>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT LayoutNGMixin<LayoutRubyBase>;

// A LayoutNG version of LayoutRubyBase.
class CORE_EXPORT LayoutNGRubyBase final
    : public LayoutNGBlockFlowMixin<LayoutRubyBase> {
 public:
  explicit LayoutNGRubyBase();
  ~LayoutNGRubyBase() override;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGRubyBase";
  }
  void UpdateBlockLayout(bool relayout_children) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_RUBY_BASE_H_
