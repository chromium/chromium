// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/semantic_embedder.h"

#include "base/task/single_thread_task_runner.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_content_embedding.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_task_type.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/availability.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

mojom::blink::AISemanticEmbedderTaskType ToMojoTaskType(
    V8SemanticEmbedderTaskType task_type) {
  switch (task_type.AsEnum()) {
    case V8SemanticEmbedderTaskType::Enum::kClustering:
      return mojom::blink::AISemanticEmbedderTaskType::kClustering;
    case V8SemanticEmbedderTaskType::Enum::kClassification:
      return mojom::blink::AISemanticEmbedderTaskType::kClassification;
    case V8SemanticEmbedderTaskType::Enum::kSemanticSimilarity:
      return mojom::blink::AISemanticEmbedderTaskType::kSemanticSimilarity;
    case V8SemanticEmbedderTaskType::Enum::kRetrievalQuery:
      return mojom::blink::AISemanticEmbedderTaskType::kRetrievalQuery;
    case V8SemanticEmbedderTaskType::Enum::kRetrievalDocument:
      return mojom::blink::AISemanticEmbedderTaskType::kRetrievalDocument;
  }
  NOTREACHED();
}

[[maybe_unused]] mojom::blink::AISemanticEmbedderEmbedOptionsPtr
EmbedOptionsToMojo(const SemanticEmbedderEmbedOptions* options) {
  auto mojo_options = mojom::blink::AISemanticEmbedderEmbedOptions::New();
  if (options && options->hasTaskType()) {
    mojo_options->task_type = ToMojoTaskType(options->taskType());
  }
  return mojo_options;
}

class CreateSemanticEmbedderClient
    : public GarbageCollected<CreateSemanticEmbedderClient>,
      public mojom::blink::AIManagerCreateSemanticEmbedderClient,
      public ExecutionContextClient {
 public:
  CreateSemanticEmbedderClient(
      ScriptState* script_state,
      ScriptPromiseResolver<SemanticEmbedder>* resolver,
      SemanticEmbedderCreateOptions* options)
      : ExecutionContextClient(ExecutionContext::From(script_state)),
        script_state_(script_state),
        resolver_(resolver),
        options_(options),
        receiver_(this, ExecutionContext::From(script_state)),
        task_runner_(ExecutionContext::From(script_state)
                         ->GetTaskRunner(TaskType::kInternalDefault)) {
    HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
        AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());

    // Stubbed out for DevTrial Phase 1
    (void)ai_manager_remote;
    Create(mojom::blink::ModelAvailabilityCheckResult::
               kUnavailableFeatureNotEnabled);
  }

  void Trace(Visitor* visitor) const override {
    ExecutionContextClient::Trace(visitor);
    visitor->Trace(script_state_);
    visitor->Trace(resolver_);
    visitor->Trace(options_);
    visitor->Trace(receiver_);
  }

  void OnResult(mojo::PendingRemote<mojom::blink::AISemanticEmbedder>
                    pending_remote) override {
    if (!resolver_) {
      return;
    }
    if (pending_remote) {
      resolver_->Resolve(MakeGarbageCollected<SemanticEmbedder>(
          script_state_, task_runner_, std::move(pending_remote), options_));
    } else {
      resolver_->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                        kExceptionMessageUnableToCreateSession);
    }
    Cleanup();
  }

  void OnError(mojom::blink::AIManagerCreateClientError error) override {
    if (!resolver_) {
      return;
    }
    resolver_->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                      kExceptionMessageUnableToCreateSession);
    Cleanup();
  }

 private:
  void Create(mojom::blink::ModelAvailabilityCheckResult result) {
    if (!resolver_) {
      return;
    }
    auto availability = ConvertModelAvailabilityCheckResult(result);
    if (availability == Availability::kUnavailable) {
      resolver_->RejectWithDOMException(
          DOMExceptionCode::kNotSupportedError,
          ConvertModelAvailabilityCheckResultToDebugString(result));
      Cleanup();
      return;
    }

    LocalDOMWindow* window = LocalDOMWindow::From(script_state_);
    if (window && RequiresUserActivation(availability) &&
        !MeetsUserActivationRequirements(window)) {
      resolver_->RejectWithDOMException(
          DOMExceptionCode::kNotAllowedError,
          kExceptionMessageUserActivationRequired);
      Cleanup();
      return;
    }

    HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
        AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());

    mojo::PendingRemote<mojom::blink::AIManagerCreateSemanticEmbedderClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   task_runner_);
    // Stubbed out for DevTrial Phase 1
    (void)ai_manager_remote;
    resolver_->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Feature not supported");
    Cleanup();
  }

  void Cleanup() {
    resolver_ = nullptr;
    receiver_.reset();
    keep_alive_.Clear();
  }

  Member<ScriptState> script_state_;
  Member<ScriptPromiseResolver<SemanticEmbedder>> resolver_;
  Member<SemanticEmbedderCreateOptions> options_;
  HeapMojoReceiver<mojom::blink::AIManagerCreateSemanticEmbedderClient,
                   CreateSemanticEmbedderClient>
      receiver_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SelfKeepAlive<CreateSemanticEmbedderClient> keep_alive_{this};
};

}  // namespace

SemanticEmbedder::SemanticEmbedder(
    ScriptState* script_state,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingRemote<mojom::blink::AISemanticEmbedder> pending_remote,
    SemanticEmbedderCreateOptions* options)
    : ExecutionContextClient(ExecutionContext::From(script_state)),
      task_runner_(std::move(task_runner)),
      embedder_remote_(ExecutionContext::From(script_state)) {
  embedder_remote_.Bind(std::move(pending_remote), task_runner_);
}

void SemanticEmbedder::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(embedder_remote_);
}

// static
ScriptPromise<V8Availability> SemanticEmbedder::availability(
    ScriptState* script_state,
    SemanticEmbedderCreateOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<V8Availability>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8Availability>>(script_state);
  auto promise = resolver->Promise();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  // TODO(crbug.com/392095655): Create a new Permissions Policy feature for
  // this.
  if (!execution_context->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kLanguageModel)) {
    resolver->Resolve(AvailabilityToV8(Availability::kUnavailable));
    return promise;
  }

  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(execution_context);

  // Stubbed out for DevTrial Phase 1
  (void)ai_manager_remote;
  resolver->Resolve(AvailabilityToV8(Availability::kUnavailable));
  return promise;
}

// static
ScriptPromise<SemanticEmbedder> SemanticEmbedder::create(
    ScriptState* script_state,
    SemanticEmbedderCreateOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<SemanticEmbedder>();
  }
  CHECK(options);

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<SemanticEmbedder>>(
          script_state);
  auto promise = resolver->Promise();

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  // TODO(crbug.com/392095655): Create a new Permissions Policy feature for
  // this.
  if (!execution_context->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kLanguageModel)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, kExceptionMessagePermissionPolicy));
    return promise;
  }

  MakeGarbageCollected<CreateSemanticEmbedderClient>(script_state, resolver,
                                                     options);
  return promise;
}

ScriptPromise<SemanticEmbedderResult> SemanticEmbedder::embed(
    ScriptState* script_state,
    const V8UnionStringOrStringSequence* input,
    const SemanticEmbedderEmbedOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<SemanticEmbedderResult>();
  }
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<SemanticEmbedderResult>>(
          script_state);
  auto promise = resolver->Promise();
  if (!embedder_remote_.is_bound()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     kExceptionMessageSessionDestroyed);
    return promise;
  }

  Vector<String> inputs;
  switch (input->GetContentType()) {
    case V8UnionStringOrStringSequence::ContentType::kString:
      inputs.push_back(input->GetAsString());
      break;
    case V8UnionStringOrStringSequence::ContentType::kStringSequence:
      inputs = input->GetAsStringSequence();
      break;
  }

  // Stubbed out
  resolver->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                   "Not implemented");
  return promise;
}

void SemanticEmbedder::destroy(ScriptState* script_state,
                               ExceptionState& exception_state) {
  embedder_remote_.reset();
}

}  // namespace blink
