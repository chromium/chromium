// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_text_control_single_line.h"

namespace blink {

LayoutNGTextControlSingleLine::LayoutNGTextControlSingleLine(Element* element)
    : LayoutNGBlockFlow(element) {}

bool LayoutNGTextControlSingleLine::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGTextControlSingleLine ||
         LayoutNGBlockFlow::IsOfType(type);
}

}  // namespace blink
