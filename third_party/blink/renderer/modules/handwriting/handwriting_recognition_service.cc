// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/handwriting/handwriting_recognition_service.h"

#include <utility>

#include "base/notreached.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_model_constraint.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_recognizer_query_result.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/handwriting/handwriting_recognizer.h"
#include "third_party/blink/renderer/modules/handwriting/handwriting_type_converters.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

void OnCreateHandwritingRecognizer(
    ScriptState* script_state,
    ScriptPromiseResolver<HandwritingRecognizer>* resolver,
    handwriting::mojom::blink::CreateHandwritingRecognizerResult result,
    mojo::PendingRemote<handwriting::mojom::blink::HandwritingRecognizer>
        pending_remote) {
  switch (result) {
    case handwriting::mojom::blink::CreateHandwritingRecognizerResult::kError: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError, "Internal error."));
      return;
    }
    case handwriting::mojom::blink::CreateHandwritingRecognizerResult::
        kNotSupported: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "The provided model constraints aren't supported."));
      return;
    }
    case handwriting::mojom::blink::CreateHandwritingRecognizerResult::kOk: {
      auto* handwriting_recognizer =
          MakeGarbageCollected<HandwritingRecognizer>(
              ExecutionContext::From(script_state), std::move(pending_remote));
      resolver->Resolve(handwriting_recognizer);
      return;
    }
  }

  NOTREACHED_IN_MIGRATION()
      << "CreateHandwritingRecognizer returns an invalid result.";
}

void OnQueryHandwritingRecognizer(
    ScriptState* script_state,
    ScriptPromiseResolver<IDLNullable<HandwritingRecognizerQueryResult>>*
        resolver,
    handwriting::mojom::blink::QueryHandwritingRecognizerResultPtr
        query_result) {
  auto* result = mojo::ConvertTo<HandwritingRecognizerQueryResult*>(
      std::move(query_result));
  resolver->Resolve(result);
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
ScriptPromise<HandwritingRecognizer>
HandwritingRecognitionService::createHandwritingRecognizer(
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

ScriptPromise<HandwritingRecognizer>
HandwritingRecognitionService::CreateHandwritingRecognizer(
    ScriptState* script_state,
    const HandwritingModelConstraint* blink_model_constraint,
    ExceptionState& exception_state) {
  if (!BootstrapMojoConnectionIfNeeded(script_state, exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<HandwritingRecognizer>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  auto mojo_model_constraint =
      handwriting::mojom::blink::HandwritingModelConstraint::New();

  for (auto const& lang : blink_model_constraint->languages()) {
    mojo_model_constraint->languages.push_back(lang);
  }

  remote_service_->CreateHandwritingRecognizer(
      std::move(mojo_model_constraint),
      WTF::BindOnce(OnCreateHandwritingRecognizer, WrapPersistent(script_state),
                    WrapPersistent(resolver)));

  return promise;
}

// static
ScriptPromise<IDLNullable<HandwritingRecognizerQueryResult>>
HandwritingRecognitionService::queryHandwritingRecognizer(
    ScriptState* script_state,
    Navigator& navigator,
    const HandwritingModelConstraint* constraint,
    ExceptionState& exception_state) {
  return HandwritingRecognitionService::From(navigator)
      .QueryHandwritingRecognizer(script_state, constraint, exception_state);
}

ScriptPromise<IDLNullable<HandwritingRecognizerQueryResult>>
HandwritingRecognitionService::QueryHandwritingRecognizer(
    ScriptState* script_state,
    const HandwritingModelConstraint* constraint,
    ExceptionState& exception_state) {
  if (!BootstrapMojoConnectionIfNeeded(script_state, exception_state)) {
    return ScriptPromise<IDLNullable<HandwritingRecognizerQueryResult>>();
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLNullable<HandwritingRecognizerQueryResult>>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  remote_service_->QueryHandwritingRecognizer(
      mojo::ConvertTo<handwriting::mojom::blink::HandwritingModelConstraintPtr>(
          constraint),
      WTF::BindOnce(&OnQueryHandwritingRecognizer, WrapPersistent(script_state),
                    WrapPersistent(resolver)));

  return promise;
}

void HandwritingRecognitionService::Trace(Visitor* visitor) const {
  visitor->Trace(remote_service_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
