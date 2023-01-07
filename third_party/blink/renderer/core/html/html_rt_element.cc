// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_rt_element.h"

#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

HTMLRTElement::HTMLRTElement(Document& document)
    : HTMLElement(html_names::kRtTag, document) {}

LayoutObject* HTMLRTElement::CreateLayoutObject(const ComputedStyle& style,
                                                LegacyLayout legacy) {
  if (style.Display() == EDisplay::kBlock)
    return LayoutObjectFactory::CreateRubyText(this, style, legacy);
  return LayoutObject::CreateObject(this, style, legacy);
}

}  // namespace blink
