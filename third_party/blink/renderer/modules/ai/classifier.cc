// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/classifier.h"

#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_writing_assistance_create_client.h"

namespace blink {

using ClassifierBase =
    AIWritingAssistanceBase<Classifier,
                            mojom::blink::AIClassifier,
                            mojom::blink::AIManagerCreateClassifierClient,
                            ClassifierCreateCoreOptionsStub,
                            ClassifierCreateOptions,
                            ClassifierClassifyOptions>;

template <>
void AIWritingAssistanceCreateClient<
    mojom::blink::AIClassifier,
    mojom::blink::AIManagerCreateClassifierClient,
    ClassifierCreateOptions,
    Classifier>::
    RemoteCreate(
        mojo::PendingRemote<mojom::blink::AIManagerCreateClassifierClient>
            client_remote) {
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());
  ai_manager_remote->CreateClassifier(
      std::move(client_remote), mojom::blink::AIClassifierCreateOptions::New(),
      monitor_ ? monitor_->BindRemote() : mojo::NullRemote());
}

template <>
void AIWritingAssistanceCreateClient<
    mojom::blink::AIClassifier,
    mojom::blink::AIManagerCreateClassifierClient,
    ClassifierCreateOptions,
    Classifier>::RemoteCanCreate(CanCreateCallback callback) {
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());
  ai_manager_remote->CanCreateClassifier(
      mojom::blink::AIClassifierCreateOptions::New(), std::move(callback));
}

// static
template <>
AIMetrics::AISessionType ClassifierBase::GetSessionType() {
  return AIMetrics::AISessionType::kClassifier;
}

// TODO(crbug.com/485366700): Create taxonomy permissions feature.
// static
template <>
network::mojom::PermissionsPolicyFeature
ClassifierBase::GetPermissionsPolicy() {
  return network::mojom::PermissionsPolicyFeature::kLanguageModel;
}

// TODO(crbug.com/484080220): Create api metrics.
// static
template <>
void ClassifierBase::RecordCreateOptionMetrics(
    const ClassifierCreateCoreOptionsStub& options,
    std::string function_name) {
  // TODO(crbug.com/484080220): To be implemented. This will eventually
  // handle metrics.
}

// static
template <>
void ClassifierBase::RemoteCanCreate(
    HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote,
    ClassifierCreateCoreOptionsStub* options,
    CanCreateCallback callback) {
  ai_manager_remote->CanCreateClassifier(
      mojom::blink::AIClassifierCreateOptions::New(), std::move(callback));
}

// static
template <>
ScriptPromise<V8Availability> ClassifierBase::availability(
    ScriptState* script_state,
    ClassifierCreateCoreOptionsStub* /*options*/,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<V8Availability>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8Availability>>(script_state);
  auto promise = resolver->Promise();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  // Return unavailable if the Permission Policy is not enabled.
  if (!execution_context->IsFeatureEnabled(GetPermissionsPolicy())) {
    resolver->Resolve(AvailabilityToV8(Availability::kUnavailable));
    return promise;
  }

  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(execution_context);

  if (!ai_manager_remote.is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  RecordCreateOptionMetrics({}, "availability");

  RemoteCanCreate(
      ai_manager_remote, nullptr,
      BindOnce(
          [](ScriptPromiseResolver<V8Availability>* resolver,
             ExecutionContext* execution_context,
             mojom::blink::ModelAvailabilityCheckResult result) {
            Availability availability = HandleModelAvailabilityCheckResult(
                execution_context, GetSessionType(), result);
            resolver->Resolve(AvailabilityToV8(availability));
          },
          WrapPersistent(resolver), WrapPersistent(execution_context)));
  return promise;
}

// static
template <>
ScriptPromise<Classifier> ClassifierBase::create(
    ScriptState* script_state,
    ClassifierCreateOptions* options,
    ExceptionState& exception_state) {
  MaybeRequestFeedback(script_state, GetSessionType());
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<Classifier>();
  }
  CHECK(options);

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<Classifier>>(script_state);
  auto promise = resolver->Promise();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  // Block access if the Permission Policy is not enabled.
  if (!execution_context->IsFeatureEnabled(GetPermissionsPolicy())) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, kExceptionMessagePermissionPolicy));
    return promise;
  }

  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(execution_context);

  if (!ai_manager_remote.is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  RecordCreateOptionMetrics({}, "create");

  MakeGarbageCollected<AIWritingAssistanceCreateClient<
      mojom::blink::AIClassifier, mojom::blink::AIManagerCreateClassifierClient,
      ClassifierCreateOptions, Classifier>>(script_state, resolver, options);
  return promise;
}

Classifier::Classifier(
    ScriptState* script_state,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingRemote<mojom::blink::AIClassifier> pending_remote,
    ClassifierCreateOptions* options)
    : ClassifierBase(script_state,
                     std::move(task_runner),
                     std::move(pending_remote),
                     std::move(options),
                     /*echo_whitespace_input=*/false) {}

void Classifier::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ClassifierBase::Trace(visitor);
}

void Classifier::remoteExecute(
    const String& input,
    const String& context,
    mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
        responder) {
  remote_->Classify(input, context, std::move(responder));
}

ScriptPromise<IDLString> Classifier::classify(
    ScriptState* script_state,
    const String& input,
    const ClassifierClassifyOptions* options,
    ExceptionState& exception_state) {
  return ClassifierBase::execute(script_state, input, options, exception_state);
}

// static
ScriptPromise<Classifier> Classifier::create(ScriptState* script_state,
                                             ClassifierCreateOptions* options,
                                             ExceptionState& exception_state) {
  return ClassifierBase::create(script_state, options, exception_state);
}

// static
ScriptPromise<V8Availability> Classifier::availability(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return ClassifierBase::availability(script_state, nullptr, exception_state);
}

}  // namespace blink
