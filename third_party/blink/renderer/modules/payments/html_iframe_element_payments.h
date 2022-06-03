// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_HTML_IFRAME_ELEMENT_PAYMENTS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_HTML_IFRAME_ELEMENT_PAYMENTS_H_

#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class HTMLIFrameElement;
class QualifiedName;

class HTMLIFrameElementPayments final
    : public GarbageCollected<HTMLIFrameElementPayments>,
      public Supplement<HTMLIFrameElement> {
 public:
  static const char kSupplementName[];

  HTMLIFrameElementPayments();

  static bool FastHasAttribute(const HTMLIFrameElement&, const QualifiedName&);
  static void SetBooleanAttribute(HTMLIFrameElement&,
                                  const QualifiedName&,
                                  bool);
  static HTMLIFrameElementPayments& From(HTMLIFrameElement&);
  static bool AllowPaymentRequest(HTMLIFrameElement&);

  void Trace(Visitor*) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_HTML_IFRAME_ELEMENT_PAYMENTS_H_
