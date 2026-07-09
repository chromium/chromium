// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/semantic_embedder.h"

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_content_embedding.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_embed_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_task_type.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/ai/ai_context_observer.h"
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

mojom::blink::AISemanticEmbedderEmbedOptionsPtr EmbedOptionsToMojo(
    const SemanticEmbedderEmbedOptions* options) {
  auto mojo_options = mojom::blink::AISemanticEmbedderEmbedOptions::New();
  if (options && options->hasTaskType()) {
    mojo_options->task_type = ToMojoTaskType(options->taskType());
  }
  return mojo_options;
}

class CreateSemanticEmbedderClient
    : public GarbageCollected<CreateSemanticEmbedderClient>,
      public mojom::blink::AIManagerCreateSemanticEmbedderClient,
      public ExecutionContextClient,
      public AIContextObserver<SemanticEmbedder> {
 public:
  CreateSemanticEmbedderClient(
      ScriptState* script_state,
      ScriptPromiseResolver<SemanticEmbedder>* resolver,
      AbortSignal* signal,
      SemanticEmbedderCreateOptions* options)
      : ExecutionContextClient(ExecutionContext::From(script_state)),
        AIContextObserver(script_state, this, resolver, signal),
        options_(options),
        receiver_(this, ExecutionContext::From(script_state)),
        task_runner_(ExecutionContext::From(script_state)
                         ->GetTaskRunner(TaskType::kInternalDefault)) {
    HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
        AIInterfaceProxy::GetAIManagerRemote(
            ExecutionContext::From(GetScriptState()));

    ai_manager_remote->CanCreateSemanticEmbedder(
        BindOnce(&CreateSemanticEmbedderClient::Create, WrapPersistent(this)));
  }

  void Trace(Visitor* visitor) const override {
    AIContextObserver<SemanticEmbedder>::Trace(visitor);
    ExecutionContextClient::Trace(visitor);
    visitor->Trace(options_);
    visitor->Trace(receiver_);
  }

  // mojom::blink::AIManagerCreateSemanticEmbedderClient:
  void OnResult(mojo::PendingRemote<mojom::blink::AISemanticEmbedder>
                    pending_remote) override {
    if (!GetResolver()) {
      return;
    }
    if (pending_remote) {
      GetResolver()->Resolve(MakeGarbageCollected<SemanticEmbedder>(
          GetScriptState(), task_runner_, std::move(pending_remote), options_));
    } else {
      GetResolver()->RejectWithDOMException(
          DOMExceptionCode::kInvalidStateError,
          kExceptionMessageUnableToCreateSession);
    }
    Cleanup();
  }

  void OnError(mojom::blink::AIManagerCreateClientError error) override {
    if (!GetResolver()) {
      return;
    }
    GetResolver()->RejectWithDOMException(
        DOMExceptionCode::kInvalidStateError,
        kExceptionMessageUnableToCreateSession);
    Cleanup();
  }

  void OnConnectionError() {
    OnError(mojom::blink::AIManagerCreateClientError::kUnableToCreateSession);
  }

 protected:
  void ResetReceiver() override { receiver_.reset(); }

 private:
  void Create(mojom::blink::ModelAvailabilityCheckResult result) {
    if (!GetResolver()) {
      return;
    }
    auto availability = ConvertModelAvailabilityCheckResult(result);
    if (availability == Availability::kUnavailable) {
      GetResolver()->RejectWithDOMException(
          DOMExceptionCode::kNotSupportedError,
          ConvertModelAvailabilityCheckResultToDebugString(result));
      Cleanup();
      return;
    }

    LocalDOMWindow* window = LocalDOMWindow::From(GetScriptState());
    if (window && RequiresUserActivation(availability) &&
        !MeetsUserActivationRequirements(window)) {
      GetResolver()->RejectWithDOMException(
          DOMExceptionCode::kNotAllowedError,
          kExceptionMessageUserActivationRequired);
      Cleanup();
      return;
    }

    HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
        AIInterfaceProxy::GetAIManagerRemote(
            ExecutionContext::From(GetScriptState()));

    mojo::PendingRemote<mojom::blink::AIManagerCreateSemanticEmbedderClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   task_runner_);
    receiver_.set_disconnect_handler(
        BindOnce(&CreateSemanticEmbedderClient::OnConnectionError,
                 WrapWeakPersistent(this)));
    ai_manager_remote->CreateSemanticEmbedder(std::move(client_remote));
  }

  Member<SemanticEmbedderCreateOptions> options_;
  HeapMojoReceiver<mojom::blink::AIManagerCreateSemanticEmbedderClient,
                   CreateSemanticEmbedderClient>
      receiver_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
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

  ai_manager_remote->CanCreateSemanticEmbedder(
      BindOnce(
          [](ScriptPromiseResolver<V8Availability>* resolver,
             ExecutionContext* execution_context,
             mojom::blink::ModelAvailabilityCheckResult result) {
            Availability availability = HandleModelAvailabilityCheckResult(
                execution_context, AIMetrics::AISessionType::kSemanticEmbedder,
                result);
            resolver->Resolve(AvailabilityToV8(availability));
          },
          WrapPersistent(resolver), WrapPersistent(execution_context))
          .Then(RejectOnDestruction(resolver)));
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

  AbortSignal* signal = options->hasSignal() ? options->signal() : nullptr;
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
                                                     signal, options);
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

  if (inputs.empty()) {
    auto* embedder_result = SemanticEmbedderResult::Create();
    embedder_result->setEmbeddings(HeapVector<Member<ContentEmbedding>>());
    resolver->Resolve(embedder_result);
    return promise;
  }

  embedder_remote_->Embed(
      inputs, EmbedOptionsToMojo(options),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          BindOnce(
              [](ScriptPromiseResolver<SemanticEmbedderResult>* resolver,
                 mojom::blink::SemanticEmbedderResultPtr result) {
                if (!result) {
                  resolver->RejectWithDOMException(
                      DOMExceptionCode::kInvalidStateError,
                      kExceptionMessageSessionDestroyed);
                  return;
                }

                auto* embedder_result = SemanticEmbedderResult::Create();

                HeapVector<Member<ContentEmbedding>> embeddings;
                embeddings.ReserveInitialCapacity(result->embeddings.size());
                for (const auto& embedding : result->embeddings) {
                  auto* content_embedding = ContentEmbedding::Create();
                  content_embedding->setValues(NotShared<DOMFloat32Array>(
                      DOMFloat32Array::Create(embedding->values)));
                  embeddings.push_back(content_embedding);
                }
                embedder_result->setEmbeddings(embeddings);

                resolver->Resolve(embedder_result);
              },
              WrapPersistent(resolver)),
          nullptr));
  return promise;
}

void SemanticEmbedder::destroy(ScriptState* script_state,
                               ExceptionState& exception_state) {
  embedder_remote_.reset();
}

}  // namespace blink
