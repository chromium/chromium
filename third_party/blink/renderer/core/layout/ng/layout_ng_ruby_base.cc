// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_ruby_base.h"

namespace blink {

LayoutNGRubyBase::LayoutNGRubyBase()
    : LayoutNGBlockFlowMixin<LayoutRubyBase>(nullptr) {}

LayoutNGRubyBase::~LayoutNGRubyBase() = default;

void LayoutNGRubyBase::UpdateBlockLayout(bool relayout_children) {
  UpdateNGBlockLayout();
}

}  // namespace blink
