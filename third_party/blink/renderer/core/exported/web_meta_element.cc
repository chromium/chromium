// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_meta_element.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

WebString WebMetaElement::ComputeEncoding() const {
  return String(ConstUnwrap<HTMLMetaElement>()->ComputeEncoding().GetName());
}

WebMetaElement::WebMetaElement(HTMLMetaElement* element)
    : WebElement(element) {}

DEFINE_WEB_NODE_TYPE_CASTS(WebMetaElement,
                           IsA<HTMLMetaElement>(ConstUnwrap<Node>()))

WebMetaElement& WebMetaElement::operator=(HTMLMetaElement* element) {
  private_ = element;
  return *this;
}

WebMetaElement::operator HTMLMetaElement*() const {
  return blink::To<HTMLMetaElement>(private_.Get());
}

}  // namespace blink
