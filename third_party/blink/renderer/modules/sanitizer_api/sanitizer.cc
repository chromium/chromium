// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sanitizer.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_sanitizer_config.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

Sanitizer* Sanitizer::Create(const SanitizerConfig* config,
                             ExceptionState& exception_state) {
  return MakeGarbageCollected<Sanitizer>(config);
}

Sanitizer::Sanitizer(const SanitizerConfig* config)
    : config_(const_cast<SanitizerConfig*>(config)) {}

Sanitizer::~Sanitizer() = default;

String Sanitizer::sanitizeToString(ScriptState* script_state,
                                   const String& input,
                                   ExceptionState& exception_state) {
  return CreateMarkup(sanitize(script_state, input, exception_state),
                      kChildrenOnly);
}

DocumentFragment* Sanitizer::sanitize(ScriptState* script_state,
                                      const String& input,
                                      ExceptionState& exception_state) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!window) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot find current DOM window.");
    return nullptr;
  }
  Document* document = window->document();
  DocumentFragment* fragment = document->createDocumentFragment();
  DCHECK(document->QuerySelector("body"));
  fragment->ParseHTML(input, document->QuerySelector("body"));
  return fragment;
}

SanitizerConfig* Sanitizer::creationOptions() const {
  return config_;
}

void Sanitizer::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(config_);
}

}  // namespace blink
