// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_WRITING_ASSISTANCE_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_WRITING_ASSISTANCE_BASE_H_

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/ai_writing_assistance_create_client.h"
#include "third_party/blink/renderer/modules/ai/availability.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace blink {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(WritingAssistanceMetricsOptionType)
enum class WritingAssistanceMetricsOptionType {
  kTldr = 0,
  kKeyPoints = 1,
  kTeaser = 2,
  kHeadline = 3,
  kMaxValue = kHeadline,

};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ai/enums.xml:WritingAssistanceMetricsOptionType)

// LINT.IfChange(WritingAssistanceMetricsOptionTone)
enum class WritingAssistanceMetricsOptionTone {
  kAsIs = 0,
  kNeutral = 1,
  kMoreFormal = 2,
  kMoreCasual = 3,
  kFormal = 4,
  kCasual = 5,
  kMaxValue = kCasual,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ai/enums.xml:WritingAssistanceMetricsOptionTone)

// LINT.IfChange(WritingAssistanceMetricsOptionFormat)
enum class WritingAssistanceMetricsOptionFormat {
  kPlainText = 0,
  kAsIs = 1,
  kMarkdown = 2,
  kMaxValue = kMarkdown,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ai/enums.xml:WritingAssistanceMetricsOptionFormat)

// LINT.IfChange(WritingAssistanceMetricsOptionLength)
enum class WritingAssistanceMetricsOptionLength {
  kShort = 0,
  kMedium = 1,
  kLong = 2,
  kAsIs = 3,
  kShorter = 4,
  kLonger = 5,
  kMaxValue = kLonger,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ai/enums.xml:WritingAssistanceMetricsOptionLength)

class ReadableStream;

using CanCreateCallback =
    base::OnceCallback<void(mojom::blink::ModelAvailabilityCheckResult)>;

// TODO(crbug.com/402442890): Consider consolidating into one remote client
// and replace it with template class.
template <typename V8SessionObjectType,
          typename AIMojoClient,
          typename AIMojoCreateClient,
          typename CreateCoreOptions,
          typename CreateOptions,
          typename ExecuteOptions>
class AIWritingAssistanceBase : public ExecutionContextClient {
 public:
  AIWritingAssistanceBase(ScriptState* script_state,
                          scoped_refptr<base::SequencedTaskRunner> task_runner,
                          mojo::PendingRemote<AIMojoClient> pending_remote,
                          CreateOptions* options,
                          bool echo_whitespace_input)
      : ExecutionContextClient(ExecutionContext::From(script_state)),
        remote_(GetExecutionContext()),
        options_(options),
        destruction_abort_controller_(AbortController::Create(script_state)),
        create_abort_signal_(options->getSignalOr(nullptr)),
        task_runner_(std::move(task_runner)),
        metric_session_type_(GetSessionType()),
        echo_whitespace_input_(echo_whitespace_input) {
    remote_.Bind(std::move(pending_remote), task_runner_);

    if (create_abort_signal_) {
      CHECK(!create_abort_signal_->aborted());
      create_abort_handle_ = create_abort_signal_->AddAlgorithm(
          BindOnce(&AIWritingAssistanceBase::OnCreateAbortSignalAborted,
                   WrapWeakPersistent(this), WrapWeakPersistent(script_state)));
    }
  }

  void Trace(Visitor* visitor) const override {
    ExecutionContextClient::Trace(visitor);
    visitor->Trace(remote_);
    visitor->Trace(options_);
    visitor->Trace(destruction_abort_controller_);
    visitor->Trace(create_abort_signal_);
    visitor->Trace(create_abort_handle_);
  }

  static ScriptPromise<V8Availability> availability(
      ScriptState* script_state,
      CreateCoreOptions* options,
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
        MakeGarbageCollected<ScriptPromiseResolver<V8Availability>>(
            script_state);
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
    RecordCreateOptionMetrics(*options, "availability");
    RemoteCanCreate(
        ai_manager_remote, options,
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

  static ScriptPromise<V8SessionObjectType> create(
      ScriptState* script_state,
      CreateOptions* options,
      ExceptionState& exception_state) {
    if (!script_state->ContextIsValid()) {
      ThrowInvalidContextException(exception_state);
      return ScriptPromise<V8SessionObjectType>();
    }
    CHECK(options);
    if (!ValidateAndCanonicalizeOptionLanguages(script_state->GetIsolate(),
                                                options)) {
      return ScriptPromise<V8SessionObjectType>();
    }

    AbortSignal* signal = options->getSignalOr(nullptr);
    if (HandleAbortSignal(signal, script_state, exception_state)) {
      return EmptyPromise();
    }

    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<V8SessionObjectType>>(
            script_state);
    auto promise = resolver->Promise();
    ExecutionContext* execution_context = ExecutionContext::From(script_state);

    // Block access if the Permission Policy is not enabled.
    if (!execution_context->IsFeatureEnabled(GetPermissionsPolicy())) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          kExceptionMessagePermissionPolicy));
      return promise;
    }

    HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
        AIInterfaceProxy::GetAIManagerRemote(execution_context);

    if (!ai_manager_remote.is_connected()) {
      RejectPromiseWithInternalError(resolver);
      return promise;
    }
    RecordCreateOptionMetrics(*options, "create");

    MakeGarbageCollected<AIWritingAssistanceCreateClient<
        AIMojoClient, AIMojoCreateClient, CreateOptions, V8SessionObjectType>>(
        script_state, resolver, options);
    return promise;
  }

  ScriptPromise<IDLString> execute(ScriptState* script_state,
                                   const String& input,
                                   const ExecuteOptions* options,
                                   ExceptionState& exception_state) {
    if (!script_state->ContextIsValid()) {
      ThrowInvalidContextException(exception_state);
      return ScriptPromise<IDLString>();
    }

    CHECK(options);
    AbortSignal* composite_signal =
        CreateCompositeSignal(script_state, options);
    if (HandleAbortSignal(composite_signal, script_state, exception_state)) {
      return EmptyPromise();
    }

    if (!remote_) {
      ThrowSessionDestroyedException(exception_state);
      return ScriptPromise<IDLString>();
    }

    base::UmaHistogramCounts1M(
        AIMetrics::GetAISessionRequestSizeMetricName(metric_session_type_),
        static_cast<int>(input.CharactersSizeInBytes()));

    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
    auto promise = resolver->Promise();

    String trimmed_input = input.StripWhiteSpace();
    if (trimmed_input.empty()) {
      resolver->Resolve(echo_whitespace_input_ ? input : trimmed_input);
      return promise;
    }

    const String trimmed_context =
        options->getContextOr(g_empty_string).StripWhiteSpace();
    // Pass persistent refs to keep this instance alive during the response.
    auto pending_remote = CreateModelExecutionResponder(
        script_state, composite_signal, task_runner_, metric_session_type_,
        BindOnce(&ResolvePromiseOnCompletion<IDLString>,
                 WrapPersistent(resolver)),
        base::DoNothingWithBoundArgs(WrapPersistent(this)),
        BindOnce(&RejectPromiseOnError<IDLString>, WrapPersistent(resolver)),
        BindOnce(&RejectPromiseOnAbort<IDLString>, WrapPersistent(resolver),
                 WrapPersistent(composite_signal),
                 WrapPersistent(script_state)));
    remoteExecute(trimmed_input, trimmed_context, std::move(pending_remote));
    return promise;
  }

  // TODO(crbug.com/402442890): Refactor common code between `execute()` and
  // `executeStreaming()`.
  ReadableStream* executeStreaming(ScriptState* script_state,
                                   const String& input,
                                   const ExecuteOptions* options,
                                   ExceptionState& exception_state) {
    if (!script_state->ContextIsValid()) {
      ThrowInvalidContextException(exception_state);
      return nullptr;
    }

    CHECK(options);
    AbortSignal* composite_signal =
        CreateCompositeSignal(script_state, options);
    if (HandleAbortSignal(composite_signal, script_state, exception_state)) {
      return nullptr;
    }

    if (!remote_) {
      ThrowSessionDestroyedException(exception_state);
      return nullptr;
    }

    base::UmaHistogramCounts1M(
        AIMetrics::GetAISessionRequestSizeMetricName(metric_session_type_),
        static_cast<int>(input.CharactersSizeInBytes()));

    String trimmed_input = input.StripWhiteSpace();
    if (trimmed_input.empty()) {
      return CreateEmptyReadableStream(script_state, metric_session_type_);
    }

    const String trimmed_context =
        options->getContextOr(g_empty_string).StripWhiteSpace();
    // Pass persistent refs to keep this instance alive during the response.
    auto [readable_stream, pending_remote] =
        CreateModelExecutionStreamingResponder(
            script_state, composite_signal, task_runner_, metric_session_type_,
            base::DoNothingWithBoundArgs(WrapPersistent(this)),
            base::DoNothingWithBoundArgs(WrapPersistent(this)));
    remoteExecute(trimmed_input, trimmed_context, std::move(pending_remote));
    return readable_stream;
  }

  ScriptPromise<IDLDouble> measureInputUsage(ScriptState* script_state,
                                             const String& input,
                                             const ExecuteOptions* options,
                                             ExceptionState& exception_state) {
    if (!script_state->ContextIsValid()) {
      ThrowInvalidContextException(exception_state);
      return ScriptPromise<IDLDouble>();
    }

    CHECK(options);
    AbortSignal* composite_signal =
        CreateCompositeSignal(script_state, options);
    if (HandleAbortSignal(composite_signal, script_state, exception_state)) {
      return EmptyPromise();
    }

    if (!remote_) {
      ThrowSessionDestroyedException(exception_state);
      return ScriptPromise<IDLDouble>();
    }

    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<IDLDouble>>(script_state);
    auto promise = resolver->Promise();
    auto reject_fn = RejectOnDestruction(resolver, composite_signal);

    remote_->MeasureUsage(
        input, options->getContextOr(g_empty_string),
        BindOnce(
            [](ScriptPromiseResolver<IDLDouble>* resolver, AbortSignal* signal,
               std::optional<uint32_t> usage) {
              ExecutionContext* context = resolver->GetExecutionContext();
              if (!context) {
                return;
              }
              if (signal && signal->aborted()) {
                resolver->Reject(signal->reason(resolver->GetScriptState()));
                return;
              }
              if (!usage.has_value()) {
                resolver->Reject(DOMException::Create(
                    kExceptionMessageUnableToCalculateUsage,
                    DOMException::GetErrorName(
                        DOMExceptionCode::kOperationError)));
                return;
              }
              resolver->Resolve(static_cast<double>(usage.value()));
            },
            WrapPersistent(resolver), WrapPersistent(composite_signal))
            .Then(std::move(reject_fn)));

    return promise;
  }

  void destroy(ScriptState* script_state, ExceptionState& exception_state) {
    if (!script_state->ContextIsValid()) {
      ThrowInvalidContextException(exception_state);
      return;
    }

    destruction_abort_controller_->abort(script_state);
    DestroyImpl();
  }

  String sharedContext() const {
    return options_->getSharedContextOr(g_empty_string);
  }

  std::optional<Vector<String>> expectedInputLanguages() const {
    if (options_->hasExpectedInputLanguages()) {
      return options_->expectedInputLanguages();
    }
    return std::nullopt;
  }

  std::optional<Vector<String>> expectedContextLanguages() const {
    if (options_->hasExpectedContextLanguages()) {
      return options_->expectedContextLanguages();
    }
    return std::nullopt;
  }

  String outputLanguage() const {
    return options_->getOutputLanguageOr(String());
  }

  double inputQuota() const {
    return static_cast<double>(
        mojom::blink::kWritingAssistanceMaxInputTokenSize);
  }

 protected:
  // Executes a writing assistance task on the remote `AIMojoClient`,
  // such as `Summarize()`, `Write()`, or `Rewrite()`.
  // Called by `execute()` or `executeStreaming()`.
  virtual void remoteExecute(
      const String& input,
      const String& context,
      mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
          responder) = 0;

  // Returns the session type; defined in template specializations.
  static AIMetrics::AISessionType GetSessionType();

  // Returns permission policy feature for session type.
  static network::mojom::PermissionsPolicyFeature GetPermissionsPolicy();

  // Record metrics for options when creating or using a session.
  static void RecordCreateOptionMetrics(const CreateCoreOptions& options,
                                        std::string function_name);

  // Runs CanCreate* for the session type; defined in template specializations.
  static void RemoteCanCreate(
      HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote,
      CreateCoreOptions* options,
      CanCreateCallback callback);

  HeapMojoRemote<AIMojoClient> remote_;
  Member<CreateOptions> options_;

 private:
  void DestroyImpl() {
    remote_.reset();

    if (create_abort_handle_) {
      create_abort_signal_->RemoveAlgorithm(create_abort_handle_);
      create_abort_handle_ = nullptr;
    }
  }

  void OnCreateAbortSignalAborted(ScriptState* script_state) {
    if (script_state) {
      destruction_abort_controller_->abort(
          script_state, create_abort_signal_->reason(script_state));
    }
    DestroyImpl();
  }

  AbortSignal* CreateCompositeSignal(ScriptState* script_state,
                                     const ExecuteOptions* options) {
    HeapVector<Member<AbortSignal>> signals;

    signals.push_back(destruction_abort_controller_->signal());

    CHECK(options);
    if (options->hasSignal()) {
      signals.push_back(options->signal());
    }

    return MakeGarbageCollected<AbortSignal>(script_state, signals);
  }

  static bool ValidateAndCanonicalizeOptionLanguages(
      v8::Isolate* isolate,
      CreateCoreOptions* options) {
    using LanguageList = std::optional<Vector<String>>;
    if (options->hasExpectedContextLanguages()) {
      LanguageList result = ValidateAndCanonicalizeBCP47Languages(
          isolate, options->expectedContextLanguages());
      if (!result) {
        return false;
      }
      options->setExpectedContextLanguages(*result);
    }

    if (options->hasExpectedInputLanguages()) {
      LanguageList result = ValidateAndCanonicalizeBCP47Languages(
          isolate, options->expectedInputLanguages());
      if (!result) {
        return false;
      }
      options->setExpectedInputLanguages(*result);
    }

    if (options->hasOutputLanguage()) {
      LanguageList result = ValidateAndCanonicalizeBCP47Languages(
          isolate, {options->outputLanguage()});
      if (!result) {
        return false;
      }
      options->setOutputLanguage((*result)[0]);
    }
    return true;
  }

  // Abort controller triggered on destroy() or when create abort signal is
  // executed.
  Member<AbortController> destruction_abort_controller_;

  // Abort signal passed to CreateOptions.
  Member<AbortSignal> create_abort_signal_;

  // Handle to hold create_abort_signal_ callback, which is run when aborted.
  Member<AbortSignal::AlgorithmHandle> create_abort_handle_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  AIMetrics::AISessionType metric_session_type_;

  // Whether to echo back the original input if it only contains whitespace.
  // If false, it returns an empty string.
  bool echo_whitespace_input_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_WRITING_ASSISTANCE_BASE_H_
