// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_rt_element.h"

#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_text.h"

namespace blink {

HTMLRTElement::HTMLRTElement(Document& document)
    : HTMLElement(html_names::kRtTag, document) {}

LayoutObject* HTMLRTElement::CreateLayoutObject(const ComputedStyle& style,
                                                LegacyLayout legacy) {
  if (style.Display() == EDisplay::kBlock)
    return new LayoutRubyText(this);
  return LayoutObject::CreateObject(this, style, legacy);
}

}  // namespace blink
