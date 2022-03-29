// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/browsing_topics/browsing_topics_document_supplement.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/permissions_policy/document_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_browsing_topic.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"

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
    : Supplement<Document>(document),
      document_host_(document.GetExecutionContext()) {}

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

  if (RuntimeEnabledFeatures::PrivacySandboxAdsAPIsEnabled(
          document.GetExecutionContext())) {
    UseCounter::Count(document,
                      mojom::blink::WebFeature::kPrivacySandboxAdsAPIs);
  }

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // See https://github.com/jkarlin/topics#specific-details for the restrictions
  // on the context.

  if (document.GetExecutionContext()->GetSecurityOrigin()->IsOpaque()) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidAccessError,
        "document.browsingTopics() is not allowed in an opaque origin "
        "context."));

    return promise;
  }

  if (!document.GetFrame()->GetPage()->MainFrame()->IsOutermostMainFrame() ||
      document.GetFrame()->GetPage()->IsPrerendering()) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidAccessError,
        "document.browsingTopics() is only allowed in the primary main frame "
        "or in its child iframes."));

    return promise;
  }

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

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (!document_host_.is_bound()) {
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        document_host_.BindNewPipeAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }

  document_host_->GetBrowsingTopics(WTF::Bind(
      [](ScriptPromiseResolver* resolver,
         BrowsingTopicsDocumentSupplement* supplement,
         Vector<mojom::blink::EpochTopicPtr> browsing_topics) {
        DCHECK(resolver);
        DCHECK(supplement);

        HeapVector<Member<BrowsingTopic>> result;
        for (const auto& topic : browsing_topics) {
          BrowsingTopic* result_topic = BrowsingTopic::Create();
          result_topic->setTopic(topic->topic);
          result_topic->setVersion(topic->version);
          result_topic->setConfigVersion(topic->config_version);
          result_topic->setModelVersion(topic->model_version);
          result_topic->setTaxonomyVersion(topic->taxonomy_version);
          result.push_back(result_topic);
        }

        resolver->Resolve(result);
      },
      WrapPersistent(resolver), WrapPersistent(this)));

  return promise;
}

void BrowsingTopicsDocumentSupplement::Trace(Visitor* visitor) const {
  visitor->Trace(document_host_);

  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
