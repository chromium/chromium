// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_ruby_run.h"

namespace blink {

LayoutNGRubyRun::LayoutNGRubyRun()
    : LayoutNGBlockFlowMixin<LayoutRubyRun>(nullptr) {}

LayoutNGRubyRun::~LayoutNGRubyRun() = default;

void LayoutNGRubyRun::UpdateBlockLayout(bool relayout_children) {
  // TODO(crbug.com/1069817): Implement ruby-specific layout.
  UpdateNGBlockLayout();
}

}  // namespace blink
