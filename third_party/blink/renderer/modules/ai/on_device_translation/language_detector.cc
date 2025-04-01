// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/language_detector.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_create_monitor_callback.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/ai/ai_availability.h"
#include "third_party/blink/renderer/modules/ai/ai_context_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_create_monitor.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

namespace {

template <typename T>
class RejectOnDestructionHelper {
 public:
  explicit RejectOnDestructionHelper(ScriptPromiseResolver<T>* resolver)
      : resolver_(resolver) {
    CHECK(resolver);
  }

  RejectOnDestructionHelper(const RejectOnDestructionHelper&) = delete;
  RejectOnDestructionHelper& operator=(const RejectOnDestructionHelper&) =
      delete;

  RejectOnDestructionHelper(RejectOnDestructionHelper&& other) = default;
  RejectOnDestructionHelper& operator=(RejectOnDestructionHelper&& other) =
      default;

  void Reset() { resolver_ = nullptr; }

  ~RejectOnDestructionHelper() {
    if (resolver_) {
      resolver_->Reject();
    }
  }

 private:
  Persistent<ScriptPromiseResolver<T>> resolver_;
};

template <typename T>
base::OnceClosure RejectOnDestruction(ScriptPromiseResolver<T>* resolver) {
  return WTF::BindOnce(
      [](RejectOnDestructionHelper<T> resolver_holder) {
        resolver_holder.Reset();
      },
      RejectOnDestructionHelper(resolver));
}

class LanguageDetectorCreateTask
    : public GarbageCollected<LanguageDetectorCreateTask>,
      public ExecutionContextClient,
      public AIContextObserver<LanguageDetector> {
 public:
  LanguageDetectorCreateTask(ScriptState* script_state,
                             ScriptPromiseResolver<LanguageDetector>* resolver,
                             LanguageDetectorCreateOptions* options)
      : ExecutionContextClient(ExecutionContext::From(script_state)),
        AIContextObserver(script_state,
                          this,
                          resolver,
                          options->getSignalOr(nullptr)),
        task_runner_(AIInterfaceProxy::GetTaskRunner(GetExecutionContext())),
        resolver_(resolver),
        options_(options) {
    if (options->hasMonitor()) {
      monitor_ = MakeGarbageCollected<AICreateMonitor>(GetExecutionContext(),
                                                       task_runner_);
      std::ignore = options->monitor()->Invoke(nullptr, monitor_);
    }
  }

  void Trace(Visitor* visitor) const override {
    ExecutionContextClient::Trace(visitor);
    AIContextObserver::Trace(visitor);
    visitor->Trace(resolver_);
    visitor->Trace(monitor_);
    visitor->Trace(options_);
  }

  void OnModelLoaded(base::expected<LanguageDetectionModel*,
                                    DetectLanguageError> maybe_model) {
    if (!resolver_) {
      return;
    }
    if (!maybe_model.has_value()) {
      switch (maybe_model.error()) {
        case DetectLanguageError::kUnavailable:
          resolver_->Reject("Model not available");
          break;
      }
      Cleanup();
      return;
    }
    if (monitor_) {
      // Ensure that a download completion event is sent.
      monitor_->OnDownloadProgressUpdate(kNormalizedDownloadProgressMax,
                                         kNormalizedDownloadProgressMax);
    }
    resolver_->Resolve(MakeGarbageCollected<LanguageDetector>(
        maybe_model.value(), options_, task_runner_));
    Cleanup();
  }

 private:
  void ResetReceiver() override { resolver_ = nullptr; }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  Member<AICreateMonitor> monitor_;
  Member<ScriptPromiseResolver<LanguageDetector>> resolver_;
  Member<LanguageDetectorCreateOptions> options_;
};

void OnGotStatus(
    ExecutionContext* execution_context,
    ScriptPromiseResolver<V8AIAvailability>* resolver,
    language_detection::mojom::blink::LanguageDetectionModelStatus result) {
  if (!execution_context) {
    return;
  }
  AIAvailability availability =
      HandleLanguageDetectionModelCheckResult(execution_context, result);
  resolver->Resolve(AIAvailabilityToV8(availability));
}

}  // namespace

// static
ScriptPromise<V8AIAvailability> LanguageDetector::availability(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return EmptyPromise();
  }

  ScriptPromiseResolver<V8AIAvailability>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8AIAvailability>>(
          script_state);
  ScriptPromise<V8AIAvailability> promise = resolver->Promise();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  AIInterfaceProxy::GetLanguageDetectionModelStatus(
      execution_context,
      WTF::BindOnce(&OnGotStatus, WrapWeakPersistent(execution_context),
                    WrapPersistent(resolver))
          .Then(RejectOnDestruction(resolver)));

  return promise;
}

// static
ScriptPromise<LanguageDetector> LanguageDetector::create(
    ScriptState* script_state,
    LanguageDetectorCreateOptions* options,
    ExceptionState& exception_state) {
  // TODO(crbug.com/349927087): Take `options` into account.
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return EmptyPromise();
  }

  CHECK(options);
  AbortSignal* signal = options->getSignalOr(nullptr);
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<LanguageDetector>>(
          script_state);
  LanguageDetectorCreateTask* create_task =
      MakeGarbageCollected<LanguageDetectorCreateTask>(script_state, resolver,
                                                       options);

  AIInterfaceProxy::GetLanguageDetectionModel(
      ExecutionContext::From(script_state),
      WTF::BindOnce(&LanguageDetectorCreateTask::OnModelLoaded,
                    WrapPersistent(create_task))
          .Then(RejectOnDestruction(resolver)));

  return resolver->Promise();
}

LanguageDetector::LanguageDetector(
    LanguageDetectionModel* language_detection_model,
    LanguageDetectorCreateOptions* options,
    scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner),
      language_detection_model_(language_detection_model),
      options_(options) {}

void LanguageDetector::Trace(Visitor* visitor) const {
  visitor->Trace(language_detection_model_);
  visitor->Trace(options_);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise<IDLSequence<LanguageDetectionResult>> LanguageDetector::detect(
    ScriptState* script_state,
    const WTF::String& input,
    LanguageDetectorDetectOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return ScriptPromise<IDLSequence<LanguageDetectionResult>>();
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<
      ResolverWithAbortSignal<IDLSequence<LanguageDetectionResult>>>(
      script_state, signal);

  language_detection_model_->DetectLanguage(
      task_runner_, input,
      WTF::BindOnce(LanguageDetector::OnDetectComplete,
                    WrapPersistent(resolver)));
  return resolver->Promise();
}

void LanguageDetector::destroy(ScriptState*) {
  // TODO(crbug.com/349927087): Implement the function.
}

ScriptPromise<IDLDouble> LanguageDetector::measureInputUsage(
    ScriptState* script_state,
    const WTF::String& input,
    LanguageDetectorDetectOptions* options,
    ExceptionState& exception_state) {
  // https://webmachinelearning.github.io/writing-assistance-apis/#measure-ai-model-input-usage
  //
  // If modelObject’s relevant global object is a Window whose associated
  // Document is not fully active, then return a promise rejected with an
  // "InvalidStateError" DOMException.
  auto* context = ExecutionContext::From(script_state);
  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    auto* document = window->document();
    if (document && !document->IsActive()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "The document is not active");
      return EmptyPromise();
    }
  }

  // TODO(crbug.com/399693771): This should be a composite signal of the passed
  // in abort signal and the create abort signal.
  CHECK(options);
  AbortSignal* signal = options->getSignalOr(nullptr);
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  ResolverWithAbortSignal<IDLDouble>* resolver =
      MakeGarbageCollected<ResolverWithAbortSignal<IDLDouble>>(script_state,
                                                               signal);

  task_runner_->PostTask(
      FROM_HERE,
      WTF::BindOnce(&ResolverWithAbortSignal<IDLDouble>::Resolve<double>,
                    WrapPersistent(resolver), 0));

  return resolver->Promise();
}

double LanguageDetector::inputQuota() const {
  return std::numeric_limits<double>::infinity();
}

HeapVector<Member<LanguageDetectionResult>> LanguageDetector::ConvertResult(
    WTF::Vector<LanguageDetectionModel::LanguagePrediction> predictions) {
  HeapVector<Member<LanguageDetectionResult>> result;
  for (const auto& prediction : predictions) {
    auto* one = MakeGarbageCollected<LanguageDetectionResult>();
    result.push_back(one);
    one->setDetectedLanguage(String(prediction.language));
    one->setConfidence(prediction.score);
  }
  return result;
}

void LanguageDetector::OnDetectComplete(
    ResolverWithAbortSignal<IDLSequence<LanguageDetectionResult>>* resolver,
    base::expected<WTF::Vector<LanguageDetectionModel::LanguagePrediction>,
                   DetectLanguageError> result) {
  if (resolver->aborted()) {
    return;
  }

  if (result.has_value()) {
    // Order the result from most to least confident.
    std::sort(result.value().rbegin(), result.value().rend());
    resolver->Resolve(ConvertResult(result.value()));
  } else {
    switch (result.error()) {
      case DetectLanguageError::kUnavailable:
        resolver->Reject("Model not available");
    }
  }
}

}  // namespace blink
