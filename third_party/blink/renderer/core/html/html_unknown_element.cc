// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_unknown_element.h"

namespace blink {

HTMLUnknownElement::HTMLUnknownElement(const QualifiedName& tag_name,
                                       Document& document)
    : HTMLElement(tag_name, document) {
}

}  // namespace blink
