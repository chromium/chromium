// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/scoped_view_transition.h"

namespace blink {

DOMViewTransition* ScopedViewTransition::startViewTransition(
    ScriptState*,
    Element&,
    V8ViewTransitionCallback* callback,
    ExceptionState&) {
  // TODO(crbug.com/391146469): implement.
  return nullptr;
}

DOMViewTransition* ScopedViewTransition::startViewTransition(
    ScriptState*,
    Element&,
    ViewTransitionOptions* options,
    ExceptionState&) {
  // TODO(crbug.com/391146469): implement.
  return nullptr;
}

DOMViewTransition* ScopedViewTransition::startViewTransition(ScriptState*,
                                                             Element&,
                                                             ExceptionState&) {
  // TODO(crbug.com/391146469): implement.
  return nullptr;
}

}  // namespace blink
