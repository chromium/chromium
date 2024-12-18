// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_language_detector_factory.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_create_monitor_callback.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_create_monitor.h"
#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_language_detector.h"

namespace blink {

namespace {

class RejectOnDestructionHelper {
 public:
  explicit RejectOnDestructionHelper(
      ScriptPromiseResolver<AILanguageDetectorCapabilities>* resolver)
      : resolver_(resolver) {
    CHECK(resolver);
  }

  RejectOnDestructionHelper(const RejectOnDestructionHelper&) = delete;
  RejectOnDestructionHelper& operator=(const RejectOnDestructionHelper&) =
      delete;

  RejectOnDestructionHelper(RejectOnDestructionHelper&& other) = default;
  RejectOnDestructionHelper& operator=(RejectOnDestructionHelper&& other) =
      default;

  Persistent<ScriptPromiseResolver<AILanguageDetectorCapabilities>> Take() {
    return std::move(resolver_);
  }

  ~RejectOnDestructionHelper() {
    if (resolver_) {
      resolver_->Reject();
    }
  }

 private:
  Persistent<ScriptPromiseResolver<AILanguageDetectorCapabilities>> resolver_;
};

}  // namespace

AILanguageDetectorFactory::AILanguageDetectorCreateTask::
    AILanguageDetectorCreateTask(
        ScriptPromiseResolver<AILanguageDetector>* resolver,
        ExecutionContext* execution_context,
        scoped_refptr<base::SequencedTaskRunner> task_runner,
        const AILanguageDetectorCreateOptions* options)
    : ExecutionContextClient(execution_context), resolver_(resolver) {
  if (options->hasMonitor()) {
    monitor_ =
        MakeGarbageCollected<AICreateMonitor>(execution_context, task_runner);
    std::ignore = options->monitor()->Invoke(nullptr, monitor_);
  }
}

void AILanguageDetectorFactory::AILanguageDetectorCreateTask::Trace(
    Visitor* visitor) const {
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(resolver_);
  visitor->Trace(monitor_);
}

void AILanguageDetectorFactory::AILanguageDetectorCreateTask::CreateDetector(
    base::OnceClosure on_created_callback) {
  LanguageDetectionModel::Create(
      GetExecutionContext()->GetBrowserInterfaceBroker(),
      WTF::BindOnce(&AILanguageDetectorFactory::AILanguageDetectorCreateTask::
                        OnModelLoaded,
                    WrapPersistent(this), std::move(on_created_callback)));
}

AILanguageDetectorFactory::AILanguageDetectorFactory(
    ExecutionContext* context,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ExecutionContextClient(context), task_runner_(task_runner) {}

void AILanguageDetectorFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(pending_tasks_);
}

void AILanguageDetectorFactory::AILanguageDetectorCreateTask::OnModelLoaded(
    base::OnceClosure on_created_callback,
    base::expected<LanguageDetectionModel*, DetectLanguageError> maybe_model) {
  if (maybe_model.has_value()) {
    LanguageDetectionModel* model = maybe_model.value();
    // TODO (crbug.com/383022111): Pass the real download progress rather than
    // mocking one.
    if (monitor_) {
      monitor_->OnDownloadProgressUpdate(0, model->GetModelSize());
      monitor_->OnDownloadProgressUpdate(model->GetModelSize(),
                                         model->GetModelSize());
    }
    resolver_->Resolve(MakeGarbageCollected<AILanguageDetector>(model));
  } else {
    switch (maybe_model.error()) {
      case DetectLanguageError::kUnavailable:
        resolver_->Reject("Model not available");
    }
  }

  std::move(on_created_callback).Run();
}

ScriptPromise<AILanguageDetector> AILanguageDetectorFactory::create(
    ScriptState* script_state,
    AILanguageDetectorCreateOptions* options,
    ExceptionState& exception_state) {
  // TODO(crbug.com/349927087): Take `options` into account.
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return ScriptPromise<AILanguageDetector>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AILanguageDetector>>(
          script_state);
  AILanguageDetectorCreateTask* create_task =
      MakeGarbageCollected<AILanguageDetectorCreateTask>(
          resolver, GetExecutionContext(), task_runner_, options);
  pending_tasks_.insert(create_task);
  create_task->CreateDetector(WTF::BindOnce(
      [](AILanguageDetectorFactory* factory,
         AILanguageDetectorCreateTask* task) {
        factory->pending_tasks_.erase(task);
      },
      WrapWeakPersistent(this), WrapPersistent(create_task)));
  return resolver->Promise();
}

ScriptPromise<AILanguageDetectorCapabilities>
AILanguageDetectorFactory::capabilities(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<AILanguageDetectorCapabilities>>(script_state);
  // The call may silently fail on mojo connection errors. The
  // RejectOnDestructionHelper class is created to reject the promise if
  // such error happens.
  LanguageDetectionModel::GetStatus(
      GetExecutionContext()->GetBrowserInterfaceBroker(),
      WTF::BindOnce(
          [](RejectOnDestructionHelper resolver,
             LanguageDetectionModel::LanguageDetectionModelStatus status) {
            resolver.Take()->Resolve(
                MakeGarbageCollected<AILanguageDetectorCapabilities>(status));
          },
          RejectOnDestructionHelper(resolver)));

  return resolver->Promise();
}

}  // namespace blink
