// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/browsing_topics/browsing_topics_document_supplement.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_browsing_topic.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_browsing_topics_options.h"

namespace blink {

// static
const char BrowsingTopicsDocumentSupplement::kSupplementName[] =
    "BrowsingTopicsDocumentSupplement";

// static
BrowsingTopicsDocumentSupplement* BrowsingTopicsDocumentSupplement::From(
    Document& document) {
  auto* supplement =
      Supplement<Document>::From<BrowsingTopicsDocumentSupplement>(document);
  if (!supplement) {
    supplement =
        MakeGarbageCollected<BrowsingTopicsDocumentSupplement>(document);
    Supplement<Document>::ProvideTo(document, supplement);
  }
  return supplement;
}

// static
ScriptPromise<IDLSequence<BrowsingTopic>>
BrowsingTopicsDocumentSupplement::browsingTopics(
    ScriptState* script_state,
    Document& document,
    ExceptionState& exception_state) {
  auto* supplement = From(document);
  return supplement->GetBrowsingTopics(
      script_state, document, BrowsingTopicsOptions::Create(), exception_state);
}

// static
ScriptPromise<IDLSequence<BrowsingTopic>>
BrowsingTopicsDocumentSupplement::browsingTopics(
    ScriptState* script_state,
    Document& document,
    const BrowsingTopicsOptions* options,
    ExceptionState& exception_state) {
  auto* supplement = From(document);
  return supplement->GetBrowsingTopics(script_state, document, options,
                                       exception_state);
}

BrowsingTopicsDocumentSupplement::BrowsingTopicsDocumentSupplement(
    Document& document)
    : Supplement<Document>(document) {}

ScriptPromise<IDLSequence<BrowsingTopic>>
BrowsingTopicsDocumentSupplement::GetBrowsingTopics(
    ScriptState* script_state,
    Document& document,
    const BrowsingTopicsOptions* options,
    ExceptionState& exception_state) {
  exception_state.ThrowDOMException(
      DOMExceptionCode::kNotSupportedError,
      "document.browsingTopics() is deprecated and has been removed.");
  return ScriptPromise<IDLSequence<BrowsingTopic>>();
}

void BrowsingTopicsDocumentSupplement::Trace(Visitor* visitor) const {
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
