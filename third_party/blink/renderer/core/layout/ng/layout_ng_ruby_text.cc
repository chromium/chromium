// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_ruby_text.h"

namespace blink {

LayoutNGRubyText::LayoutNGRubyText(Element* element)
    : LayoutNGBlockFlowMixin<LayoutRubyText>(element) {}

LayoutNGRubyText::~LayoutNGRubyText() = default;

void LayoutNGRubyText::UpdateBlockLayout(bool relayout_children) {
  UpdateNGBlockLayout();
}

}  // namespace blink
