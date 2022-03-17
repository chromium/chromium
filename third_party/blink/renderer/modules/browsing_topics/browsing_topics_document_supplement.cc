// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/browsing_topics/browsing_topics_document_supplement.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/permissions_policy/document_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_browsing_topic.h"

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
ScriptPromise BrowsingTopicsDocumentSupplement::browsingTopics(
    ScriptState* script_state,
    Document& document,
    ExceptionState& exception_state) {
  auto* supplement = From(document);
  return supplement->GetBrowsingTopics(script_state, document, exception_state);
}

BrowsingTopicsDocumentSupplement::BrowsingTopicsDocumentSupplement(
    Document& document)
    : Supplement<Document>(document) {}

ScriptPromise BrowsingTopicsDocumentSupplement::GetBrowsingTopics(
    ScriptState* script_state,
    Document& document,
    ExceptionState& exception_state) {
  if (!document.GetFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "A browsing context is required when "
                                      "calling document.browsingTopics().");
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!document.GetExecutionContext()->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kBrowsingTopics)) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidAccessError,
        "The \"browsing-topics\" Permissions Policy denied the use of "
        "document.browsingTopics()."));

    return promise;
  }

  if (!document.GetExecutionContext()->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::
              kBrowsingTopicsBackwardCompatible)) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidAccessError,
        "The \"interest-cohort\" Permissions Policy denied the use of "
        "document.browsingTopics()."));

    return promise;
  }

  resolver->Resolve(HeapVector<Member<BrowsingTopic>>());
  return promise;
}

void BrowsingTopicsDocumentSupplement::Trace(Visitor* visitor) const {
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
