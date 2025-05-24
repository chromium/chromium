// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/proofreader.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_writing_assistance_create_client.h"

namespace blink {

template <>
void AIWritingAssistanceCreateClient<
    mojom::blink::AIProofreader,
    mojom::blink::AIManagerCreateProofreaderClient,
    ProofreaderCreateOptions,
    Proofreader>::
    RemoteCreate(
        mojo::PendingRemote<mojom::blink::AIManagerCreateProofreaderClient>
          client_remote) {
  // TODO(crbug.com/413766815): make actual call to mojo interface in a followup CL to
  // implement the browser/mojo side.
  // HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
  //  AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());
  // ai_manager_remote->CreateProofreader(std::move(client_remote),
  //                                     ToMojoProofreaderCreateOptions(
  //                                         options_));
}

Proofreader::Proofreader(
    ExecutionContext* execution_context,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingRemote<mojom::blink::AIProofreader> pending_remote,
    ProofreaderCreateOptions* options)
    : ExecutionContextClient(execution_context),
      remote_(execution_context),
      options_(std::move(options)),
      task_runner_(std::move(task_runner)) {
  remote_.Bind(std::move(pending_remote), task_runner_);
}

void Proofreader::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(remote_);
  visitor->Trace(options_);
}

ScriptPromise<V8Availability> Proofreader::availability(
    ScriptState* script_state,
    ProofreaderCreateCoreOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<V8Availability>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8Availability>>(
          script_state);
  auto promise = resolver->Promise();

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(execution_context);

  if (!ai_manager_remote.is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  auto callback = WTF::BindOnce(
      [](ScriptPromiseResolver<V8Availability>* resolver,
         ExecutionContext* execution_context,
         mojom::blink::ModelAvailabilityCheckResult result) {
        Availability availability = HandleModelAvailabilityCheckResult(
            execution_context, AIMetrics::AISessionType::kProofreader, result);
        resolver->Resolve(AvailabilityToV8(availability));
      },
      WrapPersistent(resolver), WrapPersistent(execution_context));

  return promise;
}

ScriptPromise<Proofreader> Proofreader::create(
    ScriptState* script_state,
    ProofreaderCreateOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<Proofreader>();
  }
  CHECK(options);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<Proofreader>>(script_state);
  auto promise = resolver->Promise();
  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    resolver->Reject(signal->reason(script_state));
    return promise;
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(execution_context);

  if (!ai_manager_remote.is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  MakeGarbageCollected<AIWritingAssistanceCreateClient<
      mojom::blink::AIProofreader,
      mojom::blink::AIManagerCreateProofreaderClient,
      ProofreaderCreateOptions,
      Proofreader>>(script_state, resolver, options)->Create();
  return promise;
}

ScriptPromise<ProofreadResult> Proofreader::proofread(
    ScriptState* script_state,
    const String& proofread_task,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<ProofreadResult>();
  }
  if (!remote_) {
    ThrowSessionDestroyedException(exception_state);
    return ScriptPromise<ProofreadResult>();
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kProofreader),
      AIMetrics::AIAPI::kProofreaderProofread);
  base::UmaHistogramCounts1M(
      AIMetrics::GetAISessionRequestSizeMetricName(
          AIMetrics::AISessionType::kProofreader),
      static_cast<int>(proofread_task.CharactersSizeInBytes()));

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<ProofreadResult>>(
      script_state);
  auto promise = resolver->Promise();

  AbortSignal* signal = options_->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    resolver->Reject(signal->reason(script_state));
    return promise;
  }

  // TODO(crbug.com/413767898): Implement the proofread logic.
  auto* result = MakeGarbageCollected<ProofreadResult>();
  resolver->Resolve(result);
  return promise;
}

void Proofreader::destroy(ScriptState* script_state,
                          ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return;
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kProofreader),
      AIMetrics::AIAPI::kSessionDestroy);

  remote_.reset();
}

}  // namespace blink
