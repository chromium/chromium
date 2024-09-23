// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/browsing_topics/browsing_topics_document_supplement.h"

#include "base/metrics/histogram_functions.h"
#include "components/browsing_topics/common/common_types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/mojom/permissions_policy/document_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_browsing_topic.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_browsing_topics_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

namespace {

void RecordInvalidRequestingContextUkmMetrics(Document& document) {
  ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult2 builder(
      document.UkmSourceID());

  builder.SetFailureReason(static_cast<int64_t>(
      browsing_topics::ApiAccessResult::kInvalidRequestingContext));
  builder.Record(document.UkmRecorder());
}

}  // namespace

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
    : Supplement<Document>(document),
      document_host_(document.GetExecutionContext()) {}

ScriptPromise<IDLSequence<BrowsingTopic>>
BrowsingTopicsDocumentSupplement::GetBrowsingTopics(
    ScriptState* script_state,
    Document& document,
    const BrowsingTopicsOptions* options,
    ExceptionState& exception_state) {
  if (!document.GetFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "A browsing context is required when "
                                      "calling document.browsingTopics().");
    RecordInvalidRequestingContextUkmMetrics(document);
    return ScriptPromise<IDLSequence<BrowsingTopic>>();
  }

  UseCounter::Count(document, mojom::blink::WebFeature::kPrivacySandboxAdsAPIs);
  UseCounter::Count(document, mojom::blink::WebFeature::kTopicsAPIAll);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<BrowsingTopic>>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  // See https://github.com/jkarlin/topics#specific-details for the restrictions
  // on the context.

  if (document.GetExecutionContext()->GetSecurityOrigin()->IsOpaque()) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidAccessError,
        "document.browsingTopics() is not allowed in an opaque origin "
        "context."));

    RecordInvalidRequestingContextUkmMetrics(document);
    return promise;
  }

  // Fenced frames disallow all permissions policies which would deny this call
  // regardless, but adding this check to make the error more explicit.
  if (document.GetFrame()->IsInFencedFrameTree()) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidAccessError,
        "document.browsingTopics() is not allowed in a fenced frame."));
    RecordInvalidRequestingContextUkmMetrics(document);
    return promise;
  }

  // The Mojo requests on a prerendered page will be canceled by default. Adding
  // this check to make the error more explicit.
  if (document.GetFrame()->GetPage()->IsPrerendering()) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidAccessError,
        "document.browsingTopics() is not allowed when the page is being "
        "prerendered."));
    RecordInvalidRequestingContextUkmMetrics(document);
    return promise;
  }

  if (!document.GetExecutionContext()->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kBrowsingTopics)) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidAccessError,
        "The \"browsing-topics\" Permissions Policy denied the use of "
        "document.browsingTopics()."));

    RecordInvalidRequestingContextUkmMetrics(document);
    return promise;
  }

  if (!document.GetExecutionContext()->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::
              kBrowsingTopicsBackwardCompatible)) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidAccessError,
        "The \"interest-cohort\" Permissions Policy denied the use of "
        "document.browsingTopics()."));

    RecordInvalidRequestingContextUkmMetrics(document);
    return promise;
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (!document_host_.is_bound()) {
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        document_host_.BindNewPipeAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }

  document_host_->GetBrowsingTopics(
      /*observe=*/!options->skipObservation(),
      WTF::BindOnce(
          [](ScriptPromiseResolver<IDLSequence<BrowsingTopic>>* resolver,
             BrowsingTopicsDocumentSupplement* supplement,
             base::TimeTicks start_time,
             mojom::blink::GetBrowsingTopicsResultPtr result) {
            DCHECK(resolver);
            DCHECK(supplement);

            if (result->is_error_message()) {
              ScriptState* script_state = resolver->GetScriptState();
              ScriptState::Scope scope(script_state);

              resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
                  script_state->GetIsolate(),
                  DOMExceptionCode::kInvalidAccessError,
                  result->get_error_message()));
              return;
            }

            DCHECK(result->is_browsing_topics());

            HeapVector<Member<BrowsingTopic>> result_array;
            for (const auto& topic : result->get_browsing_topics()) {
              BrowsingTopic* result_topic = BrowsingTopic::Create();
              result_topic->setTopic(topic->topic);
              result_topic->setVersion(topic->version);
              result_topic->setConfigVersion(topic->config_version);
              result_topic->setModelVersion(topic->model_version);
              result_topic->setTaxonomyVersion(topic->taxonomy_version);
              result_array.push_back(result_topic);
            }

            base::TimeDelta time_to_resolve =
                base::TimeTicks::Now() - start_time;
            base::UmaHistogramTimes(
                "BrowsingTopics.JavaScriptAPI.TimeToResolve", time_to_resolve);

            resolver->Resolve(result_array);
          },
          WrapPersistent(resolver), WrapPersistent(this),
          base::TimeTicks::Now()));

  return promise;
}

void BrowsingTopicsDocumentSupplement::Trace(Visitor* visitor) const {
  visitor->Trace(document_host_);

  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
