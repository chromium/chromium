// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_transition_supplement.h"

#include "cc/document_transition/document_transition_request.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_transition_callback.h"
#include "third_party/blink/renderer/core/document_transition/document_transition.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

// static
const char DocumentTransitionSupplement::kSupplementName[] =
    "DocumentTransition";

// static
DocumentTransitionSupplement* DocumentTransitionSupplement::FromIfExists(
    Document& document) {
  return Supplement<Document>::From<DocumentTransitionSupplement>(document);
}

// static
DocumentTransitionSupplement* DocumentTransitionSupplement::From(
    Document& document) {
  auto* supplement =
      Supplement<Document>::From<DocumentTransitionSupplement>(document);
  if (!supplement) {
    supplement = MakeGarbageCollected<DocumentTransitionSupplement>(document);
    Supplement<Document>::ProvideTo(document, supplement);
  }
  return supplement;
}

// static
DocumentTransition* DocumentTransitionSupplement::EnsureDocumentTransition(
    Document& document) {
  auto* supplement = From(document);
  DCHECK(supplement->GetTransition());
  return supplement->GetTransition();
}

// static
void DocumentTransitionSupplement::createDocumentTransition(
    Document& document,
    V8DocumentTransitionCallback* callback) {
  auto* transition = EnsureDocumentTransition(document);
  // TODO(vmpstr): We need to figure what to do if we already have a transition.
  if (transition->HasActiveTransition())
    return;
  auto script_scope = transition->CreateScriptMutationsAllowedScope();
  callback->InvokeAndReportException(&document, transition);
}

DocumentTransition* DocumentTransitionSupplement::GetTransition() {
  return transition_;
}

DocumentTransitionSupplement::DocumentTransitionSupplement(Document& document)
    : Supplement<Document>(document),
      transition_(MakeGarbageCollected<DocumentTransition>(&document)) {}

void DocumentTransitionSupplement::Trace(Visitor* visitor) const {
  visitor->Trace(transition_);

  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
