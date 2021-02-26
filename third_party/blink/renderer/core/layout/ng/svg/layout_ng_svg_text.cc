// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_text.h"

#include "third_party/blink/renderer/core/svg/svg_text_element.h"

namespace blink {

LayoutNGSVGText::LayoutNGSVGText(Element* element)
    : LayoutNGBlockFlow(element) {
  DCHECK(IsA<SVGTextElement>(element));
}

const char* LayoutNGSVGText::GetName() const {
  NOT_DESTROYED();
  return "LayoutNGSVGText";
}

}  // namespace blink
