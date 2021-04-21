// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_META_ELEMENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_META_ELEMENT_H_

#include "third_party/blink/public/web/web_element.h"

namespace blink {

class HTMLMetaElement;

class WebMetaElement final : public WebElement {
 public:
  WebMetaElement() : WebElement() {}
  WebMetaElement(const WebMetaElement& element) = default;

  WebMetaElement& operator=(const WebMetaElement& element) {
    WebElement::Assign(element);
    return *this;
  }
  void Assign(const WebMetaElement& element) { WebElement::Assign(element); }

  BLINK_EXPORT WebString ComputeEncoding() const;

#if INSIDE_BLINK
  WebMetaElement(HTMLMetaElement*);
  WebMetaElement& operator=(HTMLMetaElement*);
  operator HTMLMetaElement*() const;
#endif
};

DECLARE_WEB_NODE_TYPE_CASTS(WebMetaElement);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_META_ELEMENT_H_
