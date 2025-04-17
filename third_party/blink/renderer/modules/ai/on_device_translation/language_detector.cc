// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/language_detector.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_create_monitor_callback.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/ai/ai_context_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_create_monitor.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/availability.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

namespace {

// TODO(crbug.com/410949688): Figure out how to retrieve these from the model.
const HashSet<String> GetSupportedLanguages() {
  return {
      "af",      "am",  "ar",  "ar-Latn", "az", "be", "bg", "bg-Latn", "bn",
      "bs",      "ca",  "ceb", "co",      "cs", "cy", "da", "de",      "el",
      "el-Latn", "en",  "eo",  "es",      "et", "eu", "fa", "fi",      "fil",
      "fr",      "fy",  "ga",  "gd",      "gl", "gu", "ha", "haw",     "hi",
      "hi-Latn", "hmn", "hr",  "ht",      "hu", "hy", "id", "ig",      "is",
      "it",      "iw",  "ja",  "ja-Latn", "jv", "ka", "kk", "km",      "kn",
      "ko",      "ku",  "ky",  "la",      "lb", "lo", "lt", "lv",      "mg",
      "mi",      "mk",  "ml",  "mn",      "mr", "ms", "mt", "my",      "ne",
      "nl",      "no",  "ny",  "pa",      "pl", "ps", "pt", "ro",      "ru",
      "ru-Latn", "sd",  "si",  "sk",      "sl", "sm", "sn", "so",      "sq",
      "sr",      "st",  "su",  "sv",      "sw", "ta", "te", "tg",      "th",
      "tr",      "uk",  "ur",  "uz",      "vi", "xh", "yi", "yo",      "zh",
      "zh-Latn", "zu",
  };
}

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

      // If an exception is thrown, don't initiate language detection model
      // download. `AICreateMonitorCallback`'s `Invoke` will automatically
      // reject the promise with the thrown exception.
      if (options->monitor()->Invoke(nullptr, monitor_).IsNothing()) {
        return;
      }
    }

    AIInterfaceProxy::GetLanguageDetectionModel(
        GetExecutionContext(),
        WTF::BindOnce(&LanguageDetectorCreateTask::OnModelLoaded,
                      WrapPersistent(this))
            .Then(RejectOnDestruction(resolver)));
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

    std::optional<Vector<String>> expected_input_languages;
    if (options_->hasExpectedInputLanguages()) {
      expected_input_languages = GetBestFitLanguages(
          GetSupportedLanguages(), options_->expectedInputLanguages());
      if (!expected_input_languages.has_value()) {
        resolver_->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kUnknownError, "Language not available"));
        return;
      }
    }

    if (!maybe_model.has_value()) {
      switch (maybe_model.error()) {
        case DetectLanguageError::kUnavailable:
          resolver_->Reject(MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kUnknownError, "Model not available"));
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
        GetScriptState(), maybe_model.value(), options_->getSignalOr(nullptr),
        std::move(expected_input_languages), task_runner_));
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
    LanguageDetectorCreateCoreOptions* options,
    ScriptPromiseResolver<V8Availability>* resolver,
    language_detection::mojom::blink::LanguageDetectionModelStatus result) {
  if (!execution_context) {
    return;
  }
  Availability availability =
      HandleLanguageDetectionModelCheckResult(execution_context, result);

  if (options->hasExpectedInputLanguages()) {
    std::optional<Vector<String>> expected_input_languages =
        GetBestFitLanguages(GetSupportedLanguages(),
                            options->expectedInputLanguages());
    if (!expected_input_languages.has_value()) {
      resolver->Resolve(AvailabilityToV8(Availability::kUnavailable));
      return;
    }
  }

  resolver->Resolve(AvailabilityToV8(availability));
}

}  // namespace

// static
ScriptPromise<V8Availability> LanguageDetector::availability(
    ScriptState* script_state,
    LanguageDetectorCreateCoreOptions* options,
    ExceptionState& exception_state) {
  if (!ValidateScriptState(script_state, exception_state)) {
    return EmptyPromise();
  }

  // TODO(crbug.com/409848465): Validate and canonicalize
  // expectedInputLanguages.

  ScriptPromiseResolver<V8Availability>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8Availability>>(script_state);
  ScriptPromise<V8Availability> promise = resolver->Promise();

  // TODO(402166942): Return unavailable if document is not allowed to use
  // language detector permission policy.

  ExecutionContext* context = ExecutionContext::From(script_state);

  AIInterfaceProxy::GetLanguageDetectionModelStatus(
      context, WTF::BindOnce(&OnGotStatus, WrapWeakPersistent(context),
                             WrapPersistent(options), WrapPersistent(resolver))
                   .Then(RejectOnDestruction(resolver)));

  return promise;
}

// static
ScriptPromise<LanguageDetector> LanguageDetector::create(
    ScriptState* script_state,
    LanguageDetectorCreateOptions* options,
    ExceptionState& exception_state) {
  if (!ValidateScriptState(script_state, exception_state)) {
    return EmptyPromise();
  }

  // TODO(crbug.com/409848465): Validate and canonicalize
  // expectedInputLanguages.

  CHECK(options);
  AbortSignal* signal = options->getSignalOr(nullptr);
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  // TODO(402166942): Reject if document is not allowed to use language detector
  // permission policy.

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<LanguageDetector>>(
          script_state);
  MakeGarbageCollected<LanguageDetectorCreateTask>(script_state, resolver,
                                                   options);

  return resolver->Promise();
}

LanguageDetector::LanguageDetector(
    ScriptState* script_state,
    LanguageDetectionModel* language_detection_model,
    AbortSignal* create_abort_signal,
    std::optional<Vector<String>> expected_input_languages,
    scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner),
      language_detection_model_(language_detection_model),
      destruction_abort_controller_(AbortController::Create(script_state)),
      create_abort_signal_(create_abort_signal),
      expected_input_languages_(std::move(expected_input_languages)) {
  if (create_abort_signal_) {
    CHECK(!create_abort_signal_->aborted());
    create_abort_handle_ = create_abort_signal_->AddAlgorithm(WTF::BindOnce(
        &LanguageDetector::OnCreateAbortSignalAborted, WrapWeakPersistent(this),
        WrapWeakPersistent(script_state)));
  }
}

void LanguageDetector::Trace(Visitor* visitor) const {
  visitor->Trace(language_detection_model_);
  visitor->Trace(destruction_abort_controller_);
  visitor->Trace(create_abort_signal_);
  visitor->Trace(create_abort_handle_);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise<IDLSequence<LanguageDetectionResult>> LanguageDetector::detect(
    ScriptState* script_state,
    const WTF::String& input,
    LanguageDetectorDetectOptions* options,
    ExceptionState& exception_state) {
  if (!ValidateScriptState(script_state, exception_state)) {
    return EmptyPromise();
  }

  AbortSignal* composite_signal = CreateCompositeSignal(script_state, options);
  if (HandleAbortSignal(composite_signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<
      ResolverWithAbortSignal<IDLSequence<LanguageDetectionResult>>>(
      script_state, composite_signal);

  language_detection_model_->DetectLanguage(
      task_runner_, input,
      WTF::BindOnce(LanguageDetector::OnDetectComplete,
                    WrapPersistent(resolver)));
  return resolver->Promise();
}

void LanguageDetector::destroy(ScriptState* script_state) {
  destruction_abort_controller_->abort(script_state);
  DestroyImpl();
}

void LanguageDetector::DestroyImpl() {
  language_detection_model_ = nullptr;
  if (create_abort_handle_) {
    create_abort_signal_->RemoveAlgorithm(create_abort_handle_);
    create_abort_handle_ = nullptr;
  }
}

void LanguageDetector::OnCreateAbortSignalAborted(ScriptState* script_state) {
  if (script_state) {
    destruction_abort_controller_->abort(
        script_state, create_abort_signal_->reason(script_state));
  }
  DestroyImpl();
}

ScriptPromise<IDLDouble> LanguageDetector::measureInputUsage(
    ScriptState* script_state,
    const WTF::String& input,
    LanguageDetectorDetectOptions* options,
    ExceptionState& exception_state) {
  if (!ValidateScriptState(script_state, exception_state)) {
    return EmptyPromise();
  }

  AbortSignal* composite_signal = CreateCompositeSignal(script_state, options);
  if (HandleAbortSignal(composite_signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  ResolverWithAbortSignal<IDLDouble>* resolver =
      MakeGarbageCollected<ResolverWithAbortSignal<IDLDouble>>(
          script_state, composite_signal);

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
  float last_score = 1;
  float cumulative_confidence = 0;

  HeapVector<Member<LanguageDetectionResult>> results;
  for (const auto& prediction : predictions) {
    CHECK_GE(prediction.score, 0);
    CHECK_LE(prediction.score, 1 - cumulative_confidence);
    CHECK_LE(prediction.score, last_score);
    last_score = prediction.score;

    if (prediction.score == 0 || prediction.language == "unknown") {
      break;
    }
    auto* result = MakeGarbageCollected<LanguageDetectionResult>();
    results.push_back(result);
    result->setDetectedLanguage(String(prediction.language));
    result->setConfidence(prediction.score);

    cumulative_confidence += prediction.score;

    if (cumulative_confidence >= 0.99) {
      break;
    }
  }

  // Append "und" to end. Set it's confidence so that the total confidences add
  // up to 1.
  auto* result = MakeGarbageCollected<LanguageDetectionResult>();
  results.push_back(result);
  result->setDetectedLanguage(String("und"));
  result->setConfidence(1 - cumulative_confidence);

  return results;
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

AbortSignal* LanguageDetector::CreateCompositeSignal(
    ScriptState* script_state,
    LanguageDetectorDetectOptions* options) {
  HeapVector<Member<AbortSignal>> signals;

  signals.push_back(destruction_abort_controller_->signal());

  CHECK(options);
  if (options->hasSignal()) {
    signals.push_back(options->signal());
  }

  return MakeGarbageCollected<AbortSignal>(script_state, signals);
}

}  // namespace blink
