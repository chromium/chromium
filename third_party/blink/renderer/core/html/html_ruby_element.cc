// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_ruby_element.h"

#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_ruby.h"

namespace blink {

HTMLRubyElement::HTMLRubyElement(Document& document)
    : HTMLElement(html_names::kRubyTag, document) {}

LayoutObject* HTMLRubyElement::CreateLayoutObject(const ComputedStyle& style,
                                                  LegacyLayout legacy) {
  if (style.Display() == EDisplay::kInline)
    return new LayoutRubyAsInline(this);
  if (style.Display() == EDisplay::kBlock)
    return new LayoutRubyAsBlock(this);
  return LayoutObject::CreateObject(this, style, legacy);
}

}  // namespace blink
