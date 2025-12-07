// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/language_detector.h"

#include "base/containers/fixed_flat_set.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_create_monitor_callback.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/ai/ai_context_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/availability.h"
#include "third_party/blink/renderer/modules/ai/create_monitor.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "v8/include/v8-isolate.h"

namespace blink {

namespace {

// TODO(crbug.com/410949688): Figure out how to retrieve these from the model.
static constexpr auto kSupportedLanguages =
    base::MakeFixedFlatSet<std::string_view>({
        "af",      "am", "ar",      "ar-Latn", "az",      "be",  "bg",
        "bg-Latn", "bn", "bs",      "ca",      "ceb",     "co",  "cs",
        "cy",      "da", "de",      "el",      "el-Latn", "en",  "eo",
        "es",      "et", "eu",      "fa",      "fi",      "fil", "fr",
        "fy",      "ga", "gd",      "gl",      "gu",      "ha",  "haw",
        "he",      "hi", "hi-Latn", "hmn",     "hr",      "ht",  "hu",
        "hy",      "id", "ig",      "is",      "it",      "ja",  "ja-Latn",
        "jv",      "ka", "kk",      "km",      "kn",      "ko",  "ku",
        "ky",      "la", "lb",      "lo",      "lt",      "lv",  "mg",
        "mi",      "mk", "ml",      "mn",      "mr",      "ms",  "mt",
        "my",      "ne", "nl",      "no",      "ny",      "pa",  "pl",
        "ps",      "pt", "ro",      "ru",      "ru-Latn", "sd",  "si",
        "sk",      "sl", "sm",      "sn",      "so",      "sq",  "sr",
        "st",      "su", "sv",      "sw",      "ta",      "te",  "tg",
        "th",      "tr", "uk",      "ur",      "uz",      "vi",  "xh",
        "yi",      "yo", "zh",      "zh-Latn", "zu",
    });

bool RequiresUserActivation(
    language_detection::mojom::blink::LanguageDetectionModelStatus result) {
  switch (result) {
    case language_detection::mojom::blink::LanguageDetectionModelStatus::
        kAfterDownload:
      return true;
    case language_detection::mojom::blink::LanguageDetectionModelStatus::
        kReadily:
    case language_detection::mojom::blink::LanguageDetectionModelStatus::
        kNotAvailable:
      return false;
  }
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
        options_(options) {
    if (options->hasMonitor()) {
      monitor_ = MakeGarbageCollected<CreateMonitor>(
          GetExecutionContext(), options->getSignalOr(nullptr), task_runner_);

      // If an exception is thrown, don't initiate language detection model
      // download. `CreateMonitorCallback`'s `Invoke` will automatically
      // reject the promise with the thrown exception.
      if (options->monitor()->Invoke(nullptr, monitor_).IsNothing()) {
        return;
      }
    }

    AIInterfaceProxy::GetLanguageDetectionModelStatus(
        GetExecutionContext(),
        BindOnce(&LanguageDetectorCreateTask::OnGotAvailability,
                 WrapPersistent(this))
            .Then(RejectOnDestruction(resolver)));
  }

  void Trace(Visitor* visitor) const override {
    ExecutionContextClient::Trace(visitor);
    AIContextObserver::Trace(visitor);
    visitor->Trace(monitor_);
    visitor->Trace(options_);
  }

  void OnGotAvailability(
      language_detection::mojom::blink::LanguageDetectionModelStatus result) {
    if (!GetResolver()) {
      return;
    }

    ScriptState* script_state = GetScriptState();
    ExecutionContext* context = ExecutionContext::From(script_state);
    LocalDOMWindow* const window = LocalDOMWindow::From(script_state);

    // The Language Detector API is only available within a window or extension
    // service worker context. User activation is not consumed by workers, as
    // they lack the ability to do so.
    CHECK(window != nullptr || context->IsServiceWorkerGlobalScope());

    if (!context->IsServiceWorkerGlobalScope() &&
        RequiresUserActivation(result) &&
        !MeetsUserActivationRequirements(window)) {
      GetResolver()->RejectWithDOMException(
          DOMExceptionCode::kNotAllowedError,
          kExceptionMessageUserActivationRequired);
      Cleanup();
      return;
    }

    AIInterfaceProxy::GetLanguageDetectionModel(
        GetExecutionContext(),
        blink::BindOnce(&LanguageDetectorCreateTask::OnModelLoaded,
                        WrapPersistent(this))
            .Then(RejectOnDestruction(GetResolver())));
  }

  void OnModelLoaded(base::expected<LanguageDetectionModel*,
                                    DetectLanguageError> maybe_model) {
    // Call `Cleanup` when this function returns.
    RunOnDestruction run_on_destruction(BindOnce(
        &LanguageDetectorCreateTask::Cleanup, WrapWeakPersistent(this)));

    if (!GetResolver()) {
      return;
    }

    std::optional<Vector<String>> expected_input_languages;
    if (options_->hasExpectedInputLanguages() &&
        !options_->expectedInputLanguages().empty()) {
      expected_input_languages = GetBestFitLanguages(
          kSupportedLanguages, options_->expectedInputLanguages());
      if (!expected_input_languages.has_value()) {
        GetResolver()->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kUnknownError, "Language not available"));
        return;
      }
    }

    if (!maybe_model.has_value()) {
      switch (maybe_model.error()) {
        case DetectLanguageError::kUnavailable:
          GetResolver()->Reject(MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kUnknownError, "Model not available"));
          break;
      }
      return;
    }
    if (monitor_) {
      // Zero must be sent.
      monitor_->OnDownloadProgressUpdate(0, kNormalizedDownloadProgressMax);

      // Abort may have been triggered by `OnDownloadProgressUpdate`.
      if (!GetResolver()) {
        return;
      }

      // Ensure that a download completion event is sent.
      monitor_->OnDownloadProgressUpdate(kNormalizedDownloadProgressMax,
                                         kNormalizedDownloadProgressMax);

      // Abort may have been triggered by `OnDownloadProgressUpdate`.
      if (!GetResolver()) {
        return;
      }
    }
    GetResolver()->Resolve(MakeGarbageCollected<LanguageDetector>(
        GetScriptState(), maybe_model.value(), GetAbortSignal(),
        std::move(expected_input_languages), task_runner_));
  }

 private:
  void ResetReceiver() override {}

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  Member<CreateMonitor> monitor_;
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
        GetBestFitLanguages(kSupportedLanguages,
                            options->expectedInputLanguages());
    if (!expected_input_languages.has_value()) {
      resolver->Resolve(AvailabilityToV8(Availability::kUnavailable));
      return;
    }
  }

  resolver->Resolve(AvailabilityToV8(availability));
}

bool ValidateAndCanonicalizeExpectedInputLanguages(
    v8::Isolate* isolate,
    LanguageDetectorCreateCoreOptions* options) {
  if (!options->hasExpectedInputLanguages()) {
    return true;
  }
  std::optional<Vector<String>> expected_input_languages =
      ValidateAndCanonicalizeBCP47Languages(isolate,
                                            options->expectedInputLanguages());
  if (!expected_input_languages.has_value()) {
    return false;
  }
  options->setExpectedInputLanguages(*expected_input_languages);
  return true;
}

}  // namespace

// static
ScriptPromise<V8Availability> LanguageDetector::availability(
    ScriptState* script_state,
    LanguageDetectorCreateCoreOptions* options,
    ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  if (!ValidateScriptState(
          script_state, exception_state,
          RuntimeEnabledFeatures::LanguageDetectionAPIForWorkersEnabled(
              context))) {
    return EmptyPromise();
  }

  if (!ValidateAndCanonicalizeExpectedInputLanguages(script_state->GetIsolate(),
                                                     options)) {
    return EmptyPromise();
  }

  ScriptPromiseResolver<V8Availability>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8Availability>>(script_state);
  ScriptPromise<V8Availability> promise = resolver->Promise();

  // Return unavailable when the permission policy is not enabled.
  if (!context->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kLanguageDetector)) {
    resolver->Resolve(AvailabilityToV8(Availability::kUnavailable));
    return promise;
  }

  AIInterfaceProxy::GetLanguageDetectionModelStatus(
      context, BindOnce(&OnGotStatus, WrapWeakPersistent(context),
                        WrapPersistent(options), WrapPersistent(resolver))
                   .Then(RejectOnDestruction(resolver)));

  return promise;
}

// static
ScriptPromise<LanguageDetector> LanguageDetector::create(
    ScriptState* script_state,
    LanguageDetectorCreateOptions* options,
    ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  if (!ValidateScriptState(
          script_state, exception_state,
          RuntimeEnabledFeatures::LanguageDetectionAPIForWorkersEnabled(
              context))) {
    return EmptyPromise();
  }

  if (!ValidateAndCanonicalizeExpectedInputLanguages(script_state->GetIsolate(),
                                                     options)) {
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

  // Block access when the permission policy is not enabled.
  if (!context->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kLanguageDetector)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, kExceptionMessagePermissionPolicy));
    return resolver->Promise();
  }

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
    create_abort_handle_ = create_abort_signal_->AddAlgorithm(
        BindOnce(&LanguageDetector::OnCreateAbortSignalAborted,
                 WrapWeakPersistent(this), WrapWeakPersistent(script_state)));
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
    const String& input,
    LanguageDetectorDetectOptions* options,
    ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  if (!ValidateScriptState(
          script_state, exception_state,
          RuntimeEnabledFeatures::LanguageDetectionAPIForWorkersEnabled(
              context))) {
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
      blink::BindOnce(LanguageDetector::OnDetectComplete,
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
    const String& input,
    LanguageDetectorDetectOptions* options,
    ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  if (!ValidateScriptState(
          script_state, exception_state,
          RuntimeEnabledFeatures::LanguageDetectionAPIForWorkersEnabled(
              context))) {
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
      FROM_HERE, BindOnce(&ResolverWithAbortSignal<IDLDouble>::Resolve<double>,
                          WrapPersistent(resolver), 0));

  return resolver->Promise();
}

double LanguageDetector::inputQuota() const {
  return std::numeric_limits<double>::infinity();
}

HeapVector<Member<LanguageDetectionResult>> LanguageDetector::ConvertResult(
    Vector<LanguageDetectionModel::LanguagePrediction> predictions) {
  double last_score = 1;
  double cumulative_confidence = 0;

  // TODO(crbug.com/419881396): `LanguageDetectionModel::PredictWithScan` should
  // be updated to provide consistent results. Currently it only ever reports
  // the "und" language tag for the empty string. Otherwise it will report
  // unknown.
  if (predictions.size() == 1) {
    auto und = predictions.at(0);
    CHECK_EQ(und.language, "und");
    HeapVector<Member<LanguageDetectionResult>> results;
    // Append "und" to end. Set it's confidence so that the total confidences
    // add up to 1.
    auto* und_result = MakeGarbageCollected<LanguageDetectionResult>();
    results.push_back(und_result);
    und_result->setDetectedLanguage(String("und"));
    und_result->setConfidence(1);

    return results;
  }

  const auto& unknown_iter = std::find_if(
      predictions.begin(), predictions.end(),
      [](const LanguageDetectionModel::LanguagePrediction& prediction) {
        return prediction.language == "unknown";
      });

  CHECK_NE(unknown_iter, predictions.end());
  double unknown = unknown_iter->score;

  HeapVector<Member<LanguageDetectionResult>> results;
  for (const auto& prediction : predictions) {
    if (prediction.language == "unknown") {
      continue;
    }

    CHECK_GE(prediction.score, 0);
    CHECK_LE(prediction.score, 1 - cumulative_confidence);
    CHECK_LE(prediction.score, last_score);
    last_score = prediction.score;

    if (prediction.score == 0 || prediction.score < unknown) {
      break;
    }

    auto* result = MakeGarbageCollected<LanguageDetectionResult>();
    results.push_back(result);

    // The language detection model returns the outdated language code "iw"
    // instead of the canonical "he", so we correct it here.
    if (prediction.language == "iw") {
      result->setDetectedLanguage("he");
    } else {
      result->setDetectedLanguage(String(prediction.language));
    }

    result->setConfidence(prediction.score);

    cumulative_confidence += prediction.score;

    if (cumulative_confidence >= 0.99) {
      break;
    }
  }

  CHECK_GE(1 - cumulative_confidence, unknown);
  if (!results.empty()) {
    CHECK_GE(results.back()->confidence(), unknown);
  }

  // Append "und" to end. Set it's confidence so that the total confidences add
  // up to 1.
  auto* und_result = MakeGarbageCollected<LanguageDetectionResult>();
  results.push_back(und_result);
  und_result->setDetectedLanguage(String("und"));
  und_result->setConfidence(unknown);

  return results;
}

void LanguageDetector::OnDetectComplete(
    ResolverWithAbortSignal<IDLSequence<LanguageDetectionResult>>* resolver,
    base::expected<Vector<LanguageDetectionModel::LanguagePrediction>,
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
