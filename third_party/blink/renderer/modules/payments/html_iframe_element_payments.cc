// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/html_iframe_element_payments.h"

#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"

namespace blink {

// static
bool HTMLIFrameElementPayments::FastHasAttribute(
    const HTMLIFrameElement& element,
    const QualifiedName& name) {
  DCHECK(name == html_names::kAllowpaymentrequestAttr);
  return element.FastHasAttribute(name);
}

}  // namespace blink
