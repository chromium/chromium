// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"

#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

TrustedHTML::TrustedHTML(String html) : html_(std::move(html)) {}

const String& TrustedHTML::toString() const {
  return html_;
}

TrustedHTML* TrustedHTML::fromLiteral(ScriptState* script_state,
                                      const ScriptValue& templateLiteral,
                                      ExceptionState& exception_state) {
  String literal = GetTrustedTypesLiteral(templateLiteral, script_state);
  if (literal.IsNull()) {
    exception_state.ThrowTypeError("Can't fromLiteral a non-literal.");
    return nullptr;
  }

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!window) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot find current DOM window.");
    return nullptr;
  }

  // TrustedHTML::fromLiteral requires additional normalization that the other
  // trusted types do not. We want to parse the literal as if it were a
  // HTMLTemplateElement content. Ref: Step 4 of
  // https://w3c.github.io/trusted-types/dist/spec/#create-a-trusted-type-from-literal-algorithm
  HTMLTemplateElement* template_element =
      MakeGarbageCollected<HTMLTemplateElement>(*window->document());
  DCHECK(template_element->content());
  template_element->content()->ParseHTML(
      literal, template_element, ParserContentPolicy::kAllowScriptingContent);

  return MakeGarbageCollected<TrustedHTML>(template_element->innerHTML());
}

}  // namespace blink
