// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/scoped_view_transition.h"

#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"

namespace blink {

DOMViewTransition* ScopedViewTransition::startViewTransition(
    ScriptState* script_state,
    Element& element,
    V8ViewTransitionCallback* callback,
    ExceptionState& exception_state) {
  return ViewTransitionSupplement::StartViewTransitionForElement(
      script_state, &element, callback, std::nullopt, exception_state);
}

DOMViewTransition* ScopedViewTransition::startViewTransition(
    ScriptState* script_state,
    Element& element,
    ViewTransitionOptions* options,
    ExceptionState& exception_state) {
  // The generated bindings class for view_transition_options.idl ensures that
  // hasUpdate() and hasTypes() both return true.
  CHECK(!options || (options->hasUpdate() && options->hasTypes()));
  return ViewTransitionSupplement::StartViewTransitionForElement(
      script_state, &element, options ? options->update() : nullptr,
      options ? options->types() : std::nullopt, exception_state);
}

DOMViewTransition* ScopedViewTransition::startViewTransition(
    ScriptState* script_state,
    Element& element,
    ExceptionState& exception_state) {
  return ViewTransitionSupplement::StartViewTransitionForElement(
      script_state, &element, nullptr, std::nullopt, exception_state);
}

}  // namespace blink
