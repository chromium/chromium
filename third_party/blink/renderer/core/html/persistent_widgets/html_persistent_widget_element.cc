// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/persistent_widgets/html_persistent_widget_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

HTMLPersistentWidgetElement::HTMLPersistentWidgetElement(Document& document)
    : HTMLFrameOwnerElement(html_names::kPersistentwidgetTag, document) {}

void HTMLPersistentWidgetElement::ParseAttribute(
    const AttributeModificationParams& params) {
  HTMLFrameOwnerElement::ParseAttribute(params);
}

bool HTMLPersistentWidgetElement::IsURLAttribute(
    const Attribute& attribute) const {
  return attribute.GetName() == html_names::kSrcAttr ||
         HTMLFrameOwnerElement::IsURLAttribute(attribute);
}

void HTMLPersistentWidgetElement::postMessage(const ScriptValue& message,
                                              const String& target_origin,
                                              ExceptionState& exception_state) {
  // TODO(crbug.com/537818859): Implement this
}

void HTMLPersistentWidgetElement::postMessage(
    const ScriptValue& message,
    const WindowPostMessageOptions* options,
    ExceptionState& exception_state) {
  // TODO(crbug.com/537818859): Implement this
}

void HTMLPersistentWidgetElement::Trace(Visitor* visitor) const {
  HTMLFrameOwnerElement::Trace(visitor);
}

}  // namespace blink
