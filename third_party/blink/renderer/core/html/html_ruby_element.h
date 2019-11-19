// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_RUBY_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_RUBY_ELEMENT_H_

#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

// <ruby> is an HTMLElement in script, but we use a separate interface here
// so HTMLElement's createLayoutObject doesn't need to know about it.
class HTMLRubyElement final : public HTMLElement {
 public:
  explicit HTMLRubyElement(Document&);

 private:
  LayoutObject* CreateLayoutObject(const ComputedStyle&, LegacyLayout) override;
  bool TypeShouldForceLegacyLayout() const final { return true; }
};

}  // namespace blink

#endif
