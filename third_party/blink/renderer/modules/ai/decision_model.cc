// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/decision_model.h"

#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_create_monitor_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_decision_model_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_decision_model_decide_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_decision_model_decision.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_decision_model_option.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_decision_model_question.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_decision_model_question_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_expected.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_message_type.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/quota_exceeded_error.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/ai/ai_context_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/availability.h"
#include "third_party/blink/renderer/modules/ai/create_monitor.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

namespace {

// Schema limits. Keep in sync with `DecisionModelSchemaCompiler` in
// //chrome/browser/ai, which enforces the same rules in the browser.
constexpr wtf_size_t kMaxQuestions = 16;
constexpr wtf_size_t kMinChoiceOptions = 2;
constexpr wtf_size_t kMaxChoiceOptions = 26;
constexpr wtf_size_t kMinScoreOptions = 2;
constexpr wtf_size_t kMaxScoreOptions = 9;

// Validates the developer-provided schema. Throws a TypeError and returns false
// if it is malformed. When `require_questions` is false, a missing `questions`
// member is allowed (e.g. a generic `availability()` check).
bool ValidateQuestions(const DecisionModelCreateCoreOptions* options,
                       bool require_questions,
                       ExceptionState& exception_state) {
  if (!options || !options->hasQuestions()) {
    if (require_questions) {
      exception_state.ThrowTypeError(
          "DecisionModel requires a non-empty questions array.");
      return false;
    }
    return true;
  }
  if (options->questions().empty()) {
    exception_state.ThrowTypeError(
        "DecisionModel requires a non-empty questions array.");
    return false;
  }
  if (options->questions().size() > kMaxQuestions) {
    exception_state.ThrowTypeError(
        "DecisionModel supports at most 16 questions per schema.");
    return false;
  }

  HashSet<String> seen_ids;
  for (const auto& q : options->questions()) {
    if (!q || q->id().StripWhiteSpace().empty()) {
      exception_state.ThrowTypeError(
          "Each DecisionModelQuestion must have a non-empty id.");
      return false;
    }
    if (!seen_ids.insert(q->id()).is_new_entry) {
      exception_state.ThrowTypeError("Duplicate DecisionModelQuestion id: '" +
                                     q->id() + "'.");
      return false;
    }
    if (q->prompt().StripWhiteSpace().empty()) {
      exception_state.ThrowTypeError(
          "Each DecisionModelQuestion must have a non-empty prompt.");
      return false;
    }

    const bool has_opts = q->hasOptions();
    const wtf_size_t num_opts = has_opts ? q->options().size() : 0u;
    switch (q->type().AsEnum()) {
      case V8DecisionModelQuestionType::Enum::kBoolean:
        if (has_opts && num_opts > 0) {
          exception_state.ThrowTypeError(
              "Boolean DecisionModelQuestion must not specify options.");
          return false;
        }
        break;
      case V8DecisionModelQuestionType::Enum::kChoice:
        if (!has_opts || num_opts < kMinChoiceOptions ||
            num_opts > kMaxChoiceOptions) {
          exception_state.ThrowTypeError(
              "Choice DecisionModelQuestion requires between 2 and 26 "
              "options.");
          return false;
        }
        break;
      case V8DecisionModelQuestionType::Enum::kScore:
        if (has_opts &&
            (num_opts < kMinScoreOptions || num_opts > kMaxScoreOptions)) {
          exception_state.ThrowTypeError(
              "Score DecisionModelQuestion options must have between 2 and 9 "
              "levels when specified.");
          return false;
        }
        break;
    }

    if (has_opts && num_opts > 0) {
      HashSet<String> seen_labels;
      for (const auto& opt : q->options()) {
        if (!opt || opt->label().StripWhiteSpace().empty()) {
          exception_state.ThrowTypeError(
              "Each DecisionModelOption must have a non-empty label.");
          return false;
        }
        if (!seen_labels.insert(opt->label()).is_new_entry) {
          exception_state.ThrowTypeError(
              "Duplicate DecisionModelOption label: '" + opt->label() + "'.");
          return false;
        }
      }
    }
  }
  return true;
}

// TODO(crbug.com/565849508): Share expected-input language validation and
// canonicalization helper with language_model.cc and language_detector.cc.
bool ValidateAndCanonicalizeExpectedInputLanguages(
    v8::Isolate* isolate,
    DecisionModelCreateCoreOptions* options) {
  if (!options || !options->hasExpectedInputs()) {
    return true;
  }
  // Validate everything before mutating so a failure leaves `options` intact.
  const auto& expected_inputs = options->expectedInputs();
  Vector<Vector<String>> canonical_languages(expected_inputs.size());
  for (wtf_size_t i = 0; i < expected_inputs.size(); ++i) {
    if (!expected_inputs[i]->hasLanguages()) {
      continue;
    }
    std::optional<Vector<String>> canonical =
        ValidateAndCanonicalizeBCP47Languages(isolate,
                                              expected_inputs[i]->languages());
    if (!canonical.has_value()) {
      return false;
    }
    canonical_languages[i] = *std::move(canonical);
  }
  for (wtf_size_t i = 0; i < expected_inputs.size(); ++i) {
    if (!canonical_languages[i].empty()) {
      expected_inputs[i]->setLanguages(canonical_languages[i]);
    }
  }
  return true;
}

bool HasUnsupportedExpectedInputs(
    const DecisionModelCreateCoreOptions* options) {
  if (!options || !options->hasExpectedInputs()) {
    return false;
  }
  for (const auto& expected : options->expectedInputs()) {
    if (expected->type().AsEnum() != V8LanguageModelMessageType::Enum::kText) {
      return true;
    }
  }
  return false;
}

mojom::blink::AIDecisionModelQuestionType ToMojoQuestionType(
    V8DecisionModelQuestionType question_type) {
  switch (question_type.AsEnum()) {
    case V8DecisionModelQuestionType::Enum::kBoolean:
      return mojom::blink::AIDecisionModelQuestionType::kBoolean;
    case V8DecisionModelQuestionType::Enum::kChoice:
      return mojom::blink::AIDecisionModelQuestionType::kChoice;
    case V8DecisionModelQuestionType::Enum::kScore:
      return mojom::blink::AIDecisionModelQuestionType::kScore;
  }
  NOTREACHED();
}

// Converts Blink GarbageCollected IDL dictionary options directly to Mojom
// structs (matching Summarizer/Writer/LanguageModel helpers in ai_utils.cc).
mojom::blink::AIDecisionModelCreateOptionsPtr CreateOptionsToMojo(
    const DecisionModelCreateCoreOptions* options) {
  auto mojo_options = mojom::blink::AIDecisionModelCreateOptions::New();
  if (!options) {
    return mojo_options;
  }
  mojo_options->preference = ToMojoPerformancePreference(options->preference());
  if (options->hasContext()) {
    mojo_options->context = options->context();
  }
  if (options->hasExpectedInputs()) {
    Vector<mojom::blink::AILanguageCodePtr> languages;
    for (const auto& expected : options->expectedInputs()) {
      if (expected->hasLanguages()) {
        for (auto& code : ToMojoLanguageCodes(expected->languages())) {
          languages.push_back(std::move(code));
        }
      }
    }
    mojo_options->expected_input_languages = std::move(languages);
  }
  if (options->hasQuestions()) {
    for (const auto& q : options->questions()) {
      auto mojo_q = mojom::blink::AIDecisionModelQuestion::New();
      mojo_q->id = q->id();
      mojo_q->prompt = q->prompt();
      mojo_q->type = ToMojoQuestionType(q->type());
      if (q->hasOptions()) {
        for (const auto& opt : q->options()) {
          auto mojo_opt = mojom::blink::AIDecisionModelOption::New();
          mojo_opt->label = opt->label();
          if (opt->hasDescription()) {
            mojo_opt->description = opt->description();
          }
          mojo_q->options.push_back(std::move(mojo_opt));
        }
      }
      mojo_options->questions.push_back(std::move(mojo_q));
    }
  }
  return mojo_options;
}

// Manages asynchronous session creation and download progress monitoring for
// `DecisionModel.create()`.
class CreateDecisionModelClient
    : public GarbageCollected<CreateDecisionModelClient>,
      public mojom::blink::AIManagerCreateDecisionModelClient,
      public ExecutionContextClient,
      public AIContextObserver<DecisionModel> {
 public:
  CreateDecisionModelClient(ScriptState* script_state,
                            ScriptPromiseResolver<DecisionModel>* resolver,
                            AbortSignal* signal,
                            DecisionModelCreateOptions* options)
      : ExecutionContextClient(ExecutionContext::From(script_state)),
        AIContextObserver(script_state, this, resolver, signal),
        options_(options),
        receiver_(this, ExecutionContext::From(script_state)),
        task_runner_(ExecutionContext::From(script_state)
                         ->GetTaskRunner(TaskType::kInternalDefault)) {
    if (options_->hasMonitor()) {
      monitor_ = MakeGarbageCollected<CreateMonitor>(
          ExecutionContext::From(script_state), options_->getSignalOr(nullptr),
          task_runner_);
      if (options_->monitor()->Invoke(nullptr, monitor_).IsNothing()) {
        GetResolver()->Detach();
        Cleanup();
        return;
      }
    }

    HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
        AIInterfaceProxy::GetAIManagerRemote(
            ExecutionContext::From(GetScriptState()));

    ai_manager_remote->CanCreateDecisionModel(
        CreateOptionsToMojo(options_),
        BindOnce(&CreateDecisionModelClient::Create, WrapPersistent(this)));
  }

  void Trace(Visitor* visitor) const override {
    AIContextObserver<DecisionModel>::Trace(visitor);
    ExecutionContextClient::Trace(visitor);
    visitor->Trace(options_);
    visitor->Trace(receiver_);
    visitor->Trace(monitor_);
  }

  // mojom::blink::AIManagerCreateDecisionModelClient:
  void OnSessionCreated(mojo::PendingRemote<mojom::blink::AIDecisionModel>
                            pending_remote) override {
    CHECK(GetResolver());
    CHECK(pending_remote);
    GetResolver()->Resolve(MakeGarbageCollected<DecisionModel>(
        GetScriptState(), task_runner_, std::move(pending_remote), options_));
    Cleanup();
  }

  void OnError(mojom::blink::AIManagerCreateClientError error,
               mojom::blink::QuotaErrorInfoPtr quota_error_info) override {
    CHECK(GetResolver());
    switch (error) {
      case mojom::blink::AIManagerCreateClientError::kUnableToCreateSession:
      case mojom::blink::AIManagerCreateClientError::
          kUnableToCalculateTokenSize:
        GetResolver()->RejectWithDOMException(
            DOMExceptionCode::kInvalidStateError,
            kExceptionMessageUnableToCreateSession);
        break;
      case mojom::blink::AIManagerCreateClientError::kInitialInputTooLarge:
        CHECK(quota_error_info);
        QuotaExceededError::Reject(
            GetResolver(), kExceptionMessageInputTooLarge,
            static_cast<double>(quota_error_info->quota),
            static_cast<double>(quota_error_info->requested));
        break;
    }
    Cleanup();
  }

  void OnConnectionError() {
    OnError(mojom::blink::AIManagerCreateClientError::kUnableToCreateSession,
            /*quota_error_info=*/nullptr);
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

    HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
        AIInterfaceProxy::GetAIManagerRemote(
            ExecutionContext::From(GetScriptState()));

    mojo::PendingRemote<mojom::blink::AIManagerCreateDecisionModelClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   task_runner_);
    receiver_.set_disconnect_handler(
        BindOnce(&CreateDecisionModelClient::OnConnectionError,
                 WrapWeakPersistent(this)));
    ai_manager_remote->CreateDecisionModel(
        std::move(client_remote), CreateOptionsToMojo(options_),
        monitor_ ? monitor_->BindRemote() : mojo::NullRemote());
  }

  Member<DecisionModelCreateOptions> options_;
  Member<CreateMonitor> monitor_;
  HeapMojoReceiver<mojom::blink::AIManagerCreateDecisionModelClient,
                   CreateDecisionModelClient>
      receiver_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace

DecisionModel::DecisionModel(
    ScriptState* script_state,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingRemote<mojom::blink::AIDecisionModel> pending_remote,
    DecisionModelCreateOptions* options)
    : ExecutionContextClient(ExecutionContext::From(script_state)),
      task_runner_(std::move(task_runner)),
      decision_model_remote_(ExecutionContext::From(script_state)),
      // `DecisionModelCreateCoreOptions.preference` defaults to `"auto"` at the
      // WebIDL layer (matching `SummarizerCreateCoreOptions`), with `kAuto` as
      // a fallback when constructed with null options in C++ unit tests.
      preference_(options ? options->preference()
                          : V8PerformancePreference(
                                V8PerformancePreference::Enum::kAuto)) {
  decision_model_remote_.Bind(std::move(pending_remote), task_runner_);
}

void DecisionModel::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(decision_model_remote_);
}

// static
ScriptPromise<V8Availability> DecisionModel::availability(
    ScriptState* script_state,
    DecisionModelCreateCoreOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<V8Availability>();
  }
  if (!ValidateQuestions(options, /*require_questions=*/false,
                         exception_state)) {
    return ScriptPromise<V8Availability>();
  }
  if (!ValidateAndCanonicalizeExpectedInputLanguages(script_state->GetIsolate(),
                                                     options)) {
    return ScriptPromise<V8Availability>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8Availability>>(script_state);
  auto promise = resolver->Promise();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  // Intentionally shares the "language-model" Permissions-Policy feature during
  // the prototype/DevTrial phase (see explainer).
  if (!execution_context->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kLanguageModel) ||
      HasUnsupportedExpectedInputs(options)) {
    resolver->Resolve(AvailabilityToV8(Availability::kUnavailable));
    return promise;
  }

  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(execution_context);

  ai_manager_remote->CanCreateDecisionModel(
      CreateOptionsToMojo(options),
      BindOnce(
          [](ScriptPromiseResolver<V8Availability>* resolver,
             ExecutionContext* execution_context,
             mojom::blink::ModelAvailabilityCheckResult result) {
            Availability availability = HandleModelAvailabilityCheckResult(
                execution_context, AIMetrics::AISessionType::kDecisionModel,
                result);
            resolver->Resolve(AvailabilityToV8(availability));
          },
          WrapPersistent(resolver), WrapPersistent(execution_context))
          .Then(RejectOnDestruction(resolver)));
  return promise;
}

// static
ScriptPromise<DecisionModel> DecisionModel::create(
    ScriptState* script_state,
    DecisionModelCreateOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<DecisionModel>();
  }
  CHECK(options);
  if (!ValidateQuestions(options, /*require_questions=*/true,
                         exception_state)) {
    return ScriptPromise<DecisionModel>();
  }
  if (!ValidateAndCanonicalizeExpectedInputLanguages(script_state->GetIsolate(),
                                                     options)) {
    return ScriptPromise<DecisionModel>();
  }

  AbortSignal* signal = options->hasSignal() ? options->signal() : nullptr;
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<DecisionModel>>(script_state);
  auto promise = resolver->Promise();

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  // Intentionally shares the "language-model" Permissions-Policy feature during
  // the prototype/DevTrial phase (see explainer).
  if (!execution_context->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kLanguageModel)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, kExceptionMessagePermissionPolicy));
    return promise;
  }

  if (HasUnsupportedExpectedInputs(options)) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Only 'text' expectedInputs modality is currently supported.");
    return promise;
  }

  MakeGarbageCollected<CreateDecisionModelClient>(script_state, resolver,
                                                  signal, options);
  return promise;
}

ScriptPromise<IDLRecord<IDLString, DecisionModelDecision>>
DecisionModel::decide(ScriptState* script_state,
                      const String& input,
                      const DecisionModelDecideOptions* options,
                      ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<IDLRecord<IDLString, DecisionModelDecision>>();
  }

  AbortSignal* signal = options ? options->getSignalOr(nullptr) : nullptr;
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  if (input.StripWhiteSpace().empty()) {
    exception_state.ThrowTypeError(
        "DecisionModel.decide() input cannot be empty.");
    return ScriptPromise<IDLRecord<IDLString, DecisionModelDecision>>();
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLRecord<IDLString, DecisionModelDecision>>>(
      script_state);
  auto promise = resolver->Promise();
  if (!decision_model_remote_.is_bound() ||
      !decision_model_remote_.is_connected()) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The session cannot be executed because the model was deleted or the "
        "session was destroyed.");
    return promise;
  }

  AbortSignal::AlgorithmHandle* abort_handle = nullptr;
  if (signal) {
    abort_handle = signal->AddAlgorithm(BindOnce(
        [](ScriptPromiseResolver<IDLRecord<IDLString, DecisionModelDecision>>*
               resolver,
           AbortSignal* signal) {
          if (!resolver) {
            return;
          }
          ScriptState* script_state = resolver->GetScriptState();
          if (!script_state || !script_state->ContextIsValid()) {
            return;
          }
          ScriptState::Scope scope(script_state);
          resolver->Reject(signal->reason(script_state));
        },
        WrapWeakPersistent(resolver), WrapWeakPersistent(signal)));
  }

  // Pass a persistent reference to `this` so the DecisionModel and its
  // `decision_model_remote_` stay alive while `Decide()` is in flight.
  decision_model_remote_->Decide(
      input,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          BindOnce(
              [](DecisionModel*,
                 ScriptPromiseResolver<
                     IDLRecord<IDLString, DecisionModelDecision>>* resolver,
                 AbortSignal* signal,
                 AbortSignal::AlgorithmHandle* abort_handle,
                 base::expected<mojom::blink::AIDecisionModelResultPtr,
                                mojom::blink::AIDecisionModelDecideErrorPtr>
                     result) {
                if (signal && abort_handle) {
                  signal->RemoveAlgorithm(abort_handle);
                }
                if (signal && signal->aborted()) {
                  return;
                }
                if (!result.has_value()) {
                  resolver->Reject(
                      ConvertModelStreamingResponseErrorToDOMException(
                          result.error()->status,
                          std::move(result.error()->quota_error_info)));
                  return;
                }

                HeapVector<std::pair<String, Member<DecisionModelDecision>>>
                    decisions_record;
                decisions_record.ReserveInitialCapacity(
                    result.value()->decisions.size());
                for (const auto& mojo_dec : result.value()->decisions) {
                  auto* decision = DecisionModelDecision::Create();
                  decision->setId(mojo_dec->question_id);
                  decision->setLabel(mojo_dec->top_label);
                  decision->setConfidence(mojo_dec->confidence);
                  if (mojo_dec->score.has_value()) {
                    decision->setExpectedScore(*mojo_dec->score);
                  }

                  Vector<std::pair<String, float>> probs;
                  probs.ReserveInitialCapacity(mojo_dec->probabilities.size());
                  for (const auto& entry : mojo_dec->probabilities) {
                    probs.emplace_back(entry->label, entry->probability);
                  }
                  decision->setProbabilities(std::move(probs));
                  decisions_record.emplace_back(mojo_dec->question_id,
                                                decision);
                }
                resolver->Resolve(decisions_record);
              },
              WrapPersistent(this), WrapPersistent(resolver),
              WrapPersistent(signal), WrapPersistent(abort_handle)),
          base::unexpected(mojom::blink::AIDecisionModelDecideError::New(
              ModelStreamingResponseStatus::kErrorSessionDestroyed,
              /*quota_error_info=*/nullptr))));
  return promise;
}

void DecisionModel::destroy(ScriptState* script_state,
                            ExceptionState& exception_state) {
  decision_model_remote_.reset();
}

}  // namespace blink
