// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_language_detector_factory.h"

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_create_monitor_callback.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_availability.h"
#include "third_party/blink/renderer/modules/ai/ai_context_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_create_monitor.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/ai/on_device_translation/language_detector.h"
#include "third_party/blink/renderer/platform/language_detection/language_detection_model.h"

namespace blink {

namespace {

void RejectModelNotAvailable(
    ScriptPromiseResolver<LanguageDetector>* resolver) {
  resolver->Reject("Model not available");
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
  LanguageDetectorCreateTask(
      ScriptState* script_state,
      scoped_refptr<base::SequencedTaskRunner>& task_runner,
      ScriptPromiseResolver<LanguageDetector>* resolver,
      LanguageDetectionModel* model,
      const LanguageDetectorCreateOptions* options)
      : ExecutionContextClient(ExecutionContext::From(script_state)),
        AIContextObserver(script_state,
                          this,
                          resolver,
                          options->getSignalOr(nullptr)),
        task_runner_(task_runner),
        resolver_(resolver),
        language_detection_model_(model) {
    if (options->hasMonitor()) {
      monitor_ = MakeGarbageCollected<AICreateMonitor>(GetExecutionContext(),
                                                       task_runner_);
      std::ignore = options->monitor()->Invoke(nullptr, monitor_);
    }
  }

  void CreateDetector(base::File model_file) {
    if (!GetExecutionContext() || !resolver_) {
      return;
    }
    if (!model_file.IsValid()) {
      RejectModelNotAvailable(resolver_);
      return;
    }
    language_detection_model_->LoadModelFile(
        std::move(model_file),
        WTF::BindOnce(&LanguageDetectorCreateTask::OnModelLoaded,
                      WrapPersistent(this)));
  }

  void Trace(Visitor* visitor) const override {
    ExecutionContextClient::Trace(visitor);
    AIContextObserver::Trace(visitor);
    visitor->Trace(resolver_);
    visitor->Trace(monitor_);
    visitor->Trace(language_detection_model_);
  }

 private:
  void OnModelLoaded(base::expected<LanguageDetectionModel*,
                                    DetectLanguageError> maybe_model) {
    if (!resolver_) {
      return;
    }
    if (maybe_model.has_value()) {
      LanguageDetectionModel* model = maybe_model.value();
      // TODO (crbug.com/383022111): Pass the real download progress rather than
      // mocking one.
      if (monitor_) {
        monitor_->OnDownloadProgressUpdate(0, kNormalizedDownloadProgressMax);
        monitor_->OnDownloadProgressUpdate(kNormalizedDownloadProgressMax,
                                           kNormalizedDownloadProgressMax);
      }
      resolver_->Resolve(
          MakeGarbageCollected<LanguageDetector>(model, task_runner_));
    } else {
      switch (maybe_model.error()) {
        case DetectLanguageError::kUnavailable:
          RejectModelNotAvailable(resolver_);
      }
    }
  }

  void ResetReceiver() override { resolver_ = nullptr; }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  Member<AICreateMonitor> monitor_;
  Member<ScriptPromiseResolver<LanguageDetector>> resolver_;
  Member<LanguageDetectionModel> language_detection_model_;
};

}  // namespace

AILanguageDetectorFactory::AILanguageDetectorFactory(
    ExecutionContext* context,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ExecutionContextClient(context),
      task_runner_(task_runner),
      language_detection_model_(
          MakeGarbageCollected<LanguageDetectionModel>()) {}

void AILanguageDetectorFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(language_detection_model_);
}

ScriptPromise<V8AIAvailability> AILanguageDetectorFactory::availability(
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
  ExecutionContext* execution_context = GetExecutionContext();

  AIInterfaceProxy::GetLanguageDetectionDriverRemote(execution_context)
      ->GetLanguageDetectionModelStatus(
          WTF::BindOnce(&AILanguageDetectorFactory::OnGotStatus,
                        WrapPersistent(this), WrapPersistent(resolver))
              .Then(RejectOnDestruction(resolver)));

  return promise;
}

void AILanguageDetectorFactory::OnGotStatus(
    ScriptPromiseResolver<V8AIAvailability>* resolver,
    language_detection::mojom::blink::LanguageDetectionModelStatus result) {
  if (!GetExecutionContext()) {
    return;
  }
  AIAvailability availability =
      HandleLanguageDetectionModelCheckResult(GetExecutionContext(), result);
  resolver->Resolve(AIAvailabilityToV8(availability));
}

ScriptPromise<LanguageDetector> AILanguageDetectorFactory::create(
    ScriptState* script_state,
    LanguageDetectorCreateOptions* options,
    ExceptionState& exception_state) {
  // TODO(crbug.com/349927087): Take `options` into account.
  if (!script_state->ContextIsValid() || !GetExecutionContext()) {
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
      MakeGarbageCollected<LanguageDetectorCreateTask>(
          script_state, task_runner_, resolver, language_detection_model_,
          options);

  AIInterfaceProxy::GetLanguageDetectionDriverRemote(GetExecutionContext())
      ->GetLanguageDetectionModel(
          WTF::BindOnce(&LanguageDetectorCreateTask::CreateDetector,
                        WrapPersistent(create_task))
              .Then(RejectOnDestruction(resolver)));

  return resolver->Promise();
}

}  // namespace blink
