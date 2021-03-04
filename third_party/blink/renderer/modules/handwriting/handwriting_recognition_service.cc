// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/handwriting/handwriting_recognition_service.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_feature_query.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_feature_query_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_model_constraint.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/handwriting/handwriting_recognizer.h"
#include "third_party/blink/renderer/modules/handwriting/handwriting_type_converters.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

void OnCreateHandwritingRecognizer(
    ScriptState* script_state,
    ScriptPromiseResolver* resolver,
    handwriting::mojom::blink::CreateHandwritingRecognizerResult result,
    mojo::PendingRemote<handwriting::mojom::blink::HandwritingRecognizer>
        pending_remote) {
  if (result !=
      handwriting::mojom::blink::CreateHandwritingRecognizerResult::kOk) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError, "Internal error."));
    return;
  }

  auto* handwriting_recognizer = MakeGarbageCollected<HandwritingRecognizer>(
      ExecutionContext::From(script_state), std::move(pending_remote));
  resolver->Resolve(handwriting_recognizer);
}

void OnQueryHandwritingFeature(
    ScriptState* script_state,
    ScriptPromiseResolver* resolver,
    handwriting::mojom::blink::HandwritingFeatureQueryResultPtr query_result) {
  resolver->Resolve(
      mojo::ConvertTo<HandwritingFeatureQueryResult*>(std::move(query_result)));
}

}  // namespace

const char HandwritingRecognitionService::kSupplementName[] =
    "NavigatorHandwritingRecognitionService";

HandwritingRecognitionService::HandwritingRecognitionService(
    Navigator& navigator)
    : Supplement<Navigator>(navigator),
      remote_service_(navigator.GetExecutionContext()) {}

// static
HandwritingRecognitionService& HandwritingRecognitionService::From(
    Navigator& navigator) {
  HandwritingRecognitionService* supplement =
      Supplement<Navigator>::From<HandwritingRecognitionService>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<HandwritingRecognitionService>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

// static
ScriptPromise HandwritingRecognitionService::createHandwritingRecognizer(
    ScriptState* script_state,
    Navigator& navigator,
    const HandwritingModelConstraint* constraint,
    ExceptionState& exception_state) {
  return HandwritingRecognitionService::From(navigator)
      .CreateHandwritingRecognizer(script_state, constraint, exception_state);
}

bool HandwritingRecognitionService::BootstrapMojoConnectionIfNeeded(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // We need to do the following check because the execution context of this
  // navigator may be invalid (e.g. the frame is detached).
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is invalid");
    return false;
  }
  // Note that we do not use `ExecutionContext::From(script_state)` because
  // the ScriptState passed in may not be guaranteed to match the execution
  // context associated with this navigator, especially with
  // cross-browsing-context calls.
  auto* execution_context = GetSupplementable()->GetExecutionContext();
  if (!remote_service_.is_bound()) {
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        remote_service_.BindNewPipeAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kInternalDefault)));
  }
  return true;
}

ScriptPromise HandwritingRecognitionService::CreateHandwritingRecognizer(
    ScriptState* script_state,
    const HandwritingModelConstraint* blink_model_constraint,
    ExceptionState& exception_state) {
  if (!BootstrapMojoConnectionIfNeeded(script_state, exception_state)) {
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();

  auto mojo_model_constraint =
      handwriting::mojom::blink::HandwritingModelConstraint::New();

  for (auto const& lang : blink_model_constraint->languages()) {
    mojo_model_constraint->languages.push_back(lang);
  }

  remote_service_->CreateHandwritingRecognizer(
      std::move(mojo_model_constraint),
      WTF::Bind(OnCreateHandwritingRecognizer, WrapPersistent(script_state),
                WrapPersistent(resolver)));

  return promise;
}

// static
ScriptPromise HandwritingRecognitionService::queryHandwritingRecognizerSupport(
    ScriptState* script_state,
    Navigator& navigator,
    const HandwritingFeatureQuery* query,
    ExceptionState& exception_state) {
  return HandwritingRecognitionService::From(navigator)
      .QueryHandwritingRecognizerSupport(script_state, query, exception_state);
}

ScriptPromise HandwritingRecognitionService::QueryHandwritingRecognizerSupport(
    ScriptState* script_state,
    const HandwritingFeatureQuery* query,
    ExceptionState& exception_state) {
  if (!BootstrapMojoConnectionIfNeeded(script_state, exception_state)) {
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();

  remote_service_->QueryHandwritingRecognizerSupport(
      mojo::ConvertTo<handwriting::mojom::blink::HandwritingFeatureQueryPtr>(
          query),
      WTF::Bind(&OnQueryHandwritingFeature, WrapPersistent(script_state),
                WrapPersistent(resolver)));
  return promise;
}

void HandwritingRecognitionService::Trace(Visitor* visitor) const {
  visitor->Trace(remote_service_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
