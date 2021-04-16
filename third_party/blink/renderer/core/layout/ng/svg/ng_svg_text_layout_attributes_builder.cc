// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/svg/ng_svg_text_layout_attributes_builder.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"

namespace blink {

NGSVGTextLayoutAttributesBuilder::NGSVGTextLayoutAttributesBuilder(
    NGInlineNode ifc)
    : block_flow_(To<LayoutBlockFlow>(ifc.GetLayoutBox())) {}

void NGSVGTextLayoutAttributesBuilder::Build(
    const String& ifc_text_content,
    const HeapVector<NGInlineItem>& items) {}

}  // namespace blink
