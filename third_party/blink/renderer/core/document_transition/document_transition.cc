// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_transition.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_transition_init.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

DocumentTransition::DocumentTransition(Document* document,
                                       const DocumentTransitionInit* init)
    : document_(document) {}

void DocumentTransition::Trace(Visitor* visitor) const {
  visitor->Trace(document_);

  ScriptWrappable::Trace(visitor);
}

ScriptPromise DocumentTransition::prepare(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();
  resolver->Resolve();
  return promise;
}

void DocumentTransition::start() {}

}  // namespace blink
