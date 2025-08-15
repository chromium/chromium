// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/proofreader.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_writing_assistance_create_client.h"
#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

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
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());
  ai_manager_remote->CreateProofreader(
      std::move(client_remote), ToMojoProofreaderCreateOptions(options_));
}

template <>
void AIWritingAssistanceCreateClient<
    mojom::blink::AIProofreader,
    mojom::blink::AIManagerCreateProofreaderClient,
    ProofreaderCreateOptions,
    Proofreader>::RemoteCanCreate(CanCreateCallback callback) {
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());
  ai_manager_remote->CanCreateProofreader(
      ToMojoProofreaderCreateOptions(options_), std::move(callback));
}

Proofreader::Proofreader(
    ScriptState* script_state,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingRemote<mojom::blink::AIProofreader> pending_remote,
    ProofreaderCreateOptions* options)
    : ExecutionContextClient(ExecutionContext::From(script_state)),
      remote_(GetExecutionContext()),
      options_(std::move(options)),
      destruction_abort_controller_(AbortController::Create(script_state)),
      create_abort_signal_(options_->getSignalOr(nullptr)),
      task_runner_(std::move(task_runner)) {
  remote_.Bind(std::move(pending_remote), task_runner_);

  if (create_abort_signal_) {
    CHECK(!create_abort_signal_->aborted());
    create_abort_handle_ = create_abort_signal_->AddAlgorithm(WTF::BindOnce(
        &Proofreader::OnCreateAbortSignalAborted, WrapWeakPersistent(this),
        WrapWeakPersistent(script_state)));
  }
}

void Proofreader::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(remote_);
  visitor->Trace(options_);
  visitor->Trace(destruction_abort_controller_);
  visitor->Trace(create_abort_signal_);
  visitor->Trace(create_abort_handle_);
}

ScriptPromise<V8Availability> Proofreader::availability(
    ScriptState* script_state,
    ProofreaderCreateCoreOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<V8Availability>();
  }
  CHECK(options);
  if (!ValidateAndCanonicalizeOptionLanguages(script_state->GetIsolate(),
                                              options)) {
    return ScriptPromise<V8Availability>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8Availability>>(script_state);
  auto promise = resolver->Promise();

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(execution_context);

  if (!ai_manager_remote.is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  auto callback = BindOnce(
      [](ScriptPromiseResolver<V8Availability>* resolver,
         ExecutionContext* execution_context,
         mojom::blink::ModelAvailabilityCheckResult result) {
        Availability availability = HandleModelAvailabilityCheckResult(
            execution_context, AIMetrics::AISessionType::kProofreader, result);
        resolver->Resolve(AvailabilityToV8(availability));
      },
      WrapPersistent(resolver), WrapPersistent(execution_context));
  ai_manager_remote->CanCreateProofreader(
      ToMojoProofreaderCreateOptions(options), std::move(callback));

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
  if (!ValidateAndCanonicalizeOptionLanguages(script_state->GetIsolate(),
                                              options)) {
    return ScriptPromise<Proofreader>();
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<Proofreader>>(script_state);
  auto promise = resolver->Promise();

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(execution_context);

  if (!ai_manager_remote.is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  MakeGarbageCollected<AIWritingAssistanceCreateClient<
      mojom::blink::AIProofreader,
      mojom::blink::AIManagerCreateProofreaderClient, ProofreaderCreateOptions,
      Proofreader>>(script_state, resolver, options);
  return promise;
}

ScriptPromise<ProofreadResult> Proofreader::proofread(
    ScriptState* script_state,
    const String& input,
    const ProofreaderProofreadOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<ProofreadResult>();
  }

  CHECK(options);
  AbortSignal* composite_signal = CreateCompositeSignal(script_state, options);
  if (HandleAbortSignal(composite_signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  if (!remote_) {
    ThrowSessionDestroyedException(exception_state);
    return ScriptPromise<ProofreadResult>();
  }

  base::UmaHistogramCounts1M(AIMetrics::GetAISessionRequestSizeMetricName(
                                 AIMetrics::AISessionType::kProofreader),
                             static_cast<int>(input.CharactersSizeInBytes()));

  // Resolver and Promise for the final proofread() result.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<ProofreadResult>>(
      script_state);
  auto promise = resolver->Promise();

  String trimmed_input = input.StripWhiteSpace();
  if (trimmed_input.empty()) {
    auto* proofread_result = MakeGarbageCollected<ProofreadResult>();
    proofread_result->setCorrectedInput(input);
    resolver->Resolve(std::move(proofread_result));
    return promise;
  }

  // Step 1: Prompt the model to proofread and return fully corrected text.
  // Pass persistent refs to keep this instance alive during the response.
  auto pending_remote = CreateModelExecutionResponder(
      script_state, composite_signal, task_runner_,
      AIMetrics::AISessionType::kProofreader,

      BindOnce(&Proofreader::OnProofreadComplete, WrapPersistent(this),
               WrapPersistent(resolver), WrapPersistent(script_state),
               WrapPersistent(composite_signal), input),
      /*overflow_callback=*/base::DoNothingWithBoundArgs(WrapPersistent(this)),
      BindOnce(&Proofreader::OnProofreadError, WrapPersistent(this),
               WrapPersistent(resolver)),
      BindOnce(&Proofreader::OnProofreadAbort, WrapPersistent(this),
               WrapPersistent(resolver), WrapPersistent(composite_signal),
               WrapPersistent(script_state)));
  remote_->Proofread(input, std::move(pending_remote));

  return promise;
}

void Proofreader::destroy(ScriptState* script_state,
                          ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return;
  }

  remote_.reset();
  destruction_abort_controller_->abort(script_state);
  DestroyImpl();
}

// TODO(crbug.com424659255): Consolidate with AIWritingAssistanceBase.

void Proofreader::DestroyImpl() {
  remote_.reset();

  if (create_abort_handle_) {
    create_abort_signal_->RemoveAlgorithm(create_abort_handle_);
    create_abort_handle_ = nullptr;
  }
}

void Proofreader::OnCreateAbortSignalAborted(ScriptState* script_state) {
  if (script_state) {
    destruction_abort_controller_->abort(
        script_state, create_abort_signal_->reason(script_state));
  }
  DestroyImpl();
}

AbortSignal* Proofreader::CreateCompositeSignal(
    ScriptState* script_state,
    const ProofreaderProofreadOptions* options) {
  HeapVector<Member<AbortSignal>> signals;

  signals.push_back(destruction_abort_controller_->signal());

  CHECK(options);
  if (options->hasSignal()) {
    signals.push_back(options->signal());
  }

  return MakeGarbageCollected<AbortSignal>(script_state, signals);
}

bool Proofreader::ValidateAndCanonicalizeOptionLanguages(
    v8::Isolate* isolate,
    ProofreaderCreateCoreOptions* options) {
  using LanguageList = std::optional<Vector<String>>;
  if (options->hasExpectedInputLanguages()) {
    LanguageList result = ValidateAndCanonicalizeBCP47Languages(
        isolate, options->expectedInputLanguages());
    if (!result) {
      return false;
    }
    options->setExpectedInputLanguages(*result);
  }

  if (options->hasCorrectionExplanationLanguage()) {
    LanguageList result = ValidateAndCanonicalizeBCP47Languages(
        isolate, {options->correctionExplanationLanguage()});
    if (!result) {
      return false;
    }
    options->setCorrectionExplanationLanguage((*result)[0]);
  }
  return true;
}

void Proofreader::OnProofreadComplete(
    ScriptPromiseResolver<ProofreadResult>* resolver,
    ScriptState* script_state,
    AbortSignal* signal,
    const String& input,
    const String& corrected_input,
    mojom::blink::ModelExecutionContextInfoPtr context_info) {
  DCHECK(resolver);
  if (signal && signal->aborted()) {
    resolver->Reject(signal->reason(script_state));
    return;
  }
  auto* proofread_result = MakeGarbageCollected<ProofreadResult>();
  proofread_result->setCorrectedInput(corrected_input);
  resolver->Resolve(std::move(proofread_result));
}

void Proofreader::OnProofreadError(
    ScriptPromiseResolver<ProofreadResult>* resolver,
    DOMException* exception) {
  resolver->Reject(exception);
}

void Proofreader::OnProofreadAbort(
    ScriptPromiseResolver<ProofreadResult>* resolver,
    AbortSignal* signal,
    ScriptState* script_state) {
  resolver->Reject(signal->reason(script_state));
}

}  // namespace blink
