// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/html_iframe_element_payments.h"

#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

HTMLIFrameElementPayments::HTMLIFrameElementPayments() : Supplement(nullptr) {}

// static
const char HTMLIFrameElementPayments::kSupplementName[] =
    "HTMLIFrameElementPayments";

// static
bool HTMLIFrameElementPayments::FastHasAttribute(
    const HTMLIFrameElement& element,
    const QualifiedName& name) {
  DCHECK(name == html_names::kAllowpaymentrequestAttr);
  return element.FastHasAttribute(name);
}

// static
void HTMLIFrameElementPayments::SetBooleanAttribute(HTMLIFrameElement& element,
                                                    const QualifiedName& name,
                                                    bool value) {
  DCHECK(name == html_names::kAllowpaymentrequestAttr);
  element.SetBooleanAttribute(name, value);
}

// static
HTMLIFrameElementPayments& HTMLIFrameElementPayments::From(
    HTMLIFrameElement& iframe) {
  HTMLIFrameElementPayments* supplement =
      Supplement<HTMLIFrameElement>::From<HTMLIFrameElementPayments>(iframe);
  if (!supplement) {
    supplement = MakeGarbageCollected<HTMLIFrameElementPayments>();
    ProvideTo(iframe, supplement);
  }
  return *supplement;
}

// static
bool HTMLIFrameElementPayments::AllowPaymentRequest(
    HTMLIFrameElement& element) {
  return RuntimeEnabledFeatures::PaymentRequestEnabled() &&
         element.FastHasAttribute(html_names::kAllowpaymentrequestAttr);
}

void HTMLIFrameElementPayments::Trace(Visitor* visitor) const {
  Supplement<HTMLIFrameElement>::Trace(visitor);
}

}  // namespace blink
