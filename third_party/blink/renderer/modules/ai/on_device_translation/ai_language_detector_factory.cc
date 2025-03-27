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
                             const LanguageDetectorCreateOptions* options)
      : ExecutionContextClient(ExecutionContext::From(script_state)),
        AIContextObserver(script_state,
                          this,
                          resolver,
                          options->getSignalOr(nullptr)),
        task_runner_(AIInterfaceProxy::GetTaskRunner(GetExecutionContext())),
        resolver_(resolver) {
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
    // TODO (crbug.com/383022111): Pass the real download progress rather
    // than mocking one.
    if (monitor_) {
      monitor_->OnDownloadProgressUpdate(0, kNormalizedDownloadProgressMax);
      monitor_->OnDownloadProgressUpdate(kNormalizedDownloadProgressMax,
                                         kNormalizedDownloadProgressMax);
    }
    resolver_->Resolve(MakeGarbageCollected<LanguageDetector>(
        maybe_model.value(), task_runner_));
    Cleanup();
  }

 private:
  void ResetReceiver() override { resolver_ = nullptr; }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  Member<AICreateMonitor> monitor_;
  Member<ScriptPromiseResolver<LanguageDetector>> resolver_;
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

AILanguageDetectorFactory::AILanguageDetectorFactory(
    ExecutionContext* context) {}

void AILanguageDetectorFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
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
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  AIInterfaceProxy::GetLanguageDetectionModelStatus(
      execution_context,
      WTF::BindOnce(&OnGotStatus, WrapWeakPersistent(execution_context),
                    WrapPersistent(resolver))
          .Then(RejectOnDestruction(resolver)));

  return promise;
}

ScriptPromise<LanguageDetector> AILanguageDetectorFactory::create(
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

}  // namespace blink
