// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_language_detector_factory.h"

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_create_monitor_callback.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_availability.h"
#include "third_party/blink/renderer/modules/ai/ai_create_monitor.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_language_detector.h"
#include "third_party/blink/renderer/platform/language_detection/language_detection_model.h"

namespace blink {

namespace {

void RejectModelNotAvailable(
    ScriptPromiseResolver<AILanguageDetector>* resolver) {
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

  Persistent<ScriptPromiseResolver<T>> Take() { return std::move(resolver_); }

  ~RejectOnDestructionHelper() {
    if (resolver_) {
      resolver_->Reject();
    }
  }

 private:
  Persistent<ScriptPromiseResolver<T>> resolver_;
};

class AILanguageDetectorCreateTask
    : public GarbageCollected<AILanguageDetectorCreateTask> {
 public:
  AILanguageDetectorCreateTask(
      ScriptState* script_state,
      ExecutionContext* execution_context,
      scoped_refptr<base::SequencedTaskRunner>& task_runner,
      ScriptPromiseResolver<AILanguageDetector>* resolver,
      LanguageDetectionModel* model,
      const AILanguageDetectorCreateOptions* options)
      : task_runner_(task_runner),
        script_state_(script_state),
        resolver_(resolver),
        language_detection_model_(model),
        abort_signal_(options->getSignalOr(nullptr)) {
    if (options->hasMonitor()) {
      monitor_ = MakeGarbageCollected<AICreateMonitor>(execution_context,
                                                       task_runner_);
      std::ignore = options->monitor()->Invoke(nullptr, monitor_);
    }
    if (options->hasSignal()) {
      CHECK(!options->signal()->aborted());
      abort_handle_ = abort_signal_->AddAlgorithm(WTF::BindOnce(
          &AILanguageDetectorCreateTask::OnAborted, WrapWeakPersistent(this)));
    }
  }

  void CreateDetector(base::File model_file) {
    language_detection_model_->LoadModelFile(
        std::move(model_file),
        WTF::BindOnce(&AILanguageDetectorCreateTask::OnModelLoaded,
                      WrapPersistent(this)));
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(script_state_);
    visitor->Trace(resolver_);
    visitor->Trace(monitor_);
    visitor->Trace(language_detection_model_);
    visitor->Trace(abort_signal_);
    visitor->Trace(abort_handle_);
  }

 private:
  void OnModelLoaded(base::expected<LanguageDetectionModel*,
                                    DetectLanguageError> maybe_model) {
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
          MakeGarbageCollected<AILanguageDetector>(model, task_runner_));
    } else {
      switch (maybe_model.error()) {
        case DetectLanguageError::kUnavailable:
          RejectModelNotAvailable(resolver_);
      }
    }
  }
  void OnAborted() {
    if (!resolver_) {
      return;
    }
    resolver_->Reject(abort_signal_->reason(script_state_));
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  Member<ScriptState> script_state_;
  Member<AICreateMonitor> monitor_;
  Member<ScriptPromiseResolver<AILanguageDetector>> resolver_;
  Member<LanguageDetectionModel> language_detection_model_;
  Member<AbortSignal> abort_signal_;
  Member<AbortSignal::AlgorithmHandle> abort_handle_;
};

}  // namespace

AILanguageDetectorFactory::AILanguageDetectorFactory(
    ExecutionContext* context,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ExecutionContextClient(context),
      task_runner_(task_runner),
      language_detection_model_(MakeGarbageCollected<LanguageDetectionModel>()),
      language_detection_driver_(context) {}

void AILanguageDetectorFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(language_detection_model_);
  visitor->Trace(language_detection_driver_);
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

  GetLanguageDetectionDriverRemote()->GetLanguageDetectionModelStatus(
      WTF::BindOnce(
          [](RejectOnDestructionHelper<V8AIAvailability> resolver_holder,
             ExecutionContext* execution_context,
             language_detection::mojom::blink::LanguageDetectionModelStatus
                 result) {
            if (!execution_context) {
              return;
            }
            auto resolver(resolver_holder.Take());
            AIAvailability availability =
                HandleLanguageDetectionModelCheckResult(execution_context,
                                                        result);
            resolver->Resolve(AIAvailabilityToV8(availability));
          },
          RejectOnDestructionHelper(resolver),
          WrapWeakPersistent(execution_context)));

  return promise;
}

ScriptPromise<AILanguageDetector> AILanguageDetectorFactory::create(
    ScriptState* script_state,
    AILanguageDetectorCreateOptions* options,
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
      MakeGarbageCollected<ScriptPromiseResolver<AILanguageDetector>>(
          script_state);
  AILanguageDetectorCreateTask* create_task =
      MakeGarbageCollected<AILanguageDetectorCreateTask>(
          script_state, GetExecutionContext(), task_runner_, resolver,
          language_detection_model_, options);
  GetLanguageDetectionDriverRemote()->GetLanguageDetectionModel(WTF::BindOnce(
      [](RejectOnDestructionHelper<AILanguageDetector> resolver_holder,
         AILanguageDetectorCreateTask* create_task, base::File model_file) {
        auto resolver(resolver_holder.Take());
        if (!model_file.IsValid()) {
          RejectModelNotAvailable(resolver);
        }
        create_task->CreateDetector(std::move(model_file));
      },
      RejectOnDestructionHelper(resolver), WrapPersistent(create_task)));

  return resolver->Promise();
}

HeapMojoRemote<
    language_detection::mojom::blink::ContentLanguageDetectionDriver>&
AILanguageDetectorFactory::GetLanguageDetectionDriverRemote() {
  ExecutionContext* execution_context = GetExecutionContext();
  CHECK(execution_context);  // Caller should assure this.
  if (!language_detection_driver_.is_bound()) {
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        language_detection_driver_.BindNewPipeAndPassReceiver(task_runner_));
  }
  return language_detection_driver_;
}

}  // namespace blink
