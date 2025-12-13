// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_WRITING_ASSISTANCE_CREATE_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_WRITING_ASSISTANCE_CREATE_CLIENT_H_

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_create_monitor_callback.h"
#include "third_party/blink/renderer/core/dom/quota_exceeded_error.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/ai/ai_context_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_features.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/create_monitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

using CanCreateCallback =
    base::OnceCallback<void(mojom::blink::ModelAvailabilityCheckResult)>;

// TODO(crbug.com/416021087): Consolidate with LanguageModelCreateClient.
template <typename AIMojoClient,
          typename AIMojoCreateClient,
          typename CreateOptions,
          typename V8SessionObjectType>
class AIWritingAssistanceCreateClient
    : public GarbageCollected<
          AIWritingAssistanceCreateClient<AIMojoClient,
                                          AIMojoCreateClient,
                                          CreateOptions,
                                          V8SessionObjectType>>,
      public AIMojoCreateClient,
      public ExecutionContextClient,
      public AIContextObserver<V8SessionObjectType> {
 public:
  AIWritingAssistanceCreateClient(
      ScriptState* script_state,
      ScriptPromiseResolver<V8SessionObjectType>* resolver,
      CreateOptions* options)
      : ExecutionContextClient(ExecutionContext::From(script_state)),
        AIContextObserver<V8SessionObjectType>(script_state,
                                               this,
                                               resolver,
                                               options->getSignalOr(nullptr)),
        options_(options),
        receiver_(this, GetExecutionContext()),
        task_runner_(
            GetExecutionContext()->GetTaskRunner(TaskType::kInternalDefault)) {
    if (options->hasMonitor()) {
      monitor_ = MakeGarbageCollected<CreateMonitor>(
          GetExecutionContext(), options->getSignalOr(nullptr), task_runner_);
      // If an exception is thrown, don't initiate the model download.
      // `AICreateMonitorCallback`'s `Invoke` will automatically reject the
      // promise with the thrown exception.
      if (options->monitor()->Invoke(nullptr, monitor_).IsNothing()) {
        return;
      }
      HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
          AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());
      ai_manager_remote->AddModelDownloadProgressObserver(
          monitor_->BindRemote());
    }

    RemoteCanCreate(BindOnce(&AIWritingAssistanceCreateClient::Create,
                             WrapPersistent(this)));
  }
  ~AIWritingAssistanceCreateClient() override = default;

  AIWritingAssistanceCreateClient(const AIWritingAssistanceCreateClient&) =
      delete;
  AIWritingAssistanceCreateClient& operator=(
      const AIWritingAssistanceCreateClient&) = delete;

  void Trace(Visitor* visitor) const override {
    ExecutionContextClient::Trace(visitor);
    AIContextObserver<V8SessionObjectType>::Trace(visitor);
    visitor->Trace(receiver_);
    visitor->Trace(options_);
    visitor->Trace(monitor_);
  }

  // AIMojoCreateClient:
  void OnResult(mojo::PendingRemote<AIMojoClient> pending_remote) override {
    // Call `Cleanup` when this function returns.
    RunOnDestruction run_on_destruction(BindOnce(
        &AIWritingAssistanceCreateClient::Cleanup, WrapWeakPersistent(this)));

    if (!this->GetResolver()) {
      return;
    }

    if (pending_remote && monitor_) {
      // Ensure that a download completion event is sent.
      monitor_->OnDownloadProgressUpdate(0, kNormalizedDownloadProgressMax);

      // Abort may have been triggered by `OnDownloadProgressUpdate`.
      if (!this->GetResolver()) {
        return;
      }

      // Ensure that a download completion event is sent.
      monitor_->OnDownloadProgressUpdate(kNormalizedDownloadProgressMax,
                                         kNormalizedDownloadProgressMax);

      // Abort may have been triggered by `OnDownloadProgressUpdate`.
      if (!this->GetResolver()) {
        return;
      }
    }

    if (pending_remote) {
      this->GetResolver()->Resolve(MakeGarbageCollected<V8SessionObjectType>(
          this->GetScriptState(), task_runner_, std::move(pending_remote),
          options_));
    } else {
      this->GetResolver()->RejectWithDOMException(
          DOMExceptionCode::kInvalidStateError,
          kExceptionMessageUnableToCreateSession);
    }
  }

  void OnError(mojom::blink::AIManagerCreateClientError error,
               mojom::blink::QuotaErrorInfoPtr quota_error_info) override {
    // Call `Cleanup` when this function returns.
    RunOnDestruction run_on_destruction(BindOnce(
        &AIWritingAssistanceCreateClient::Cleanup, WrapWeakPersistent(this)));

    if (!this->GetResolver()) {
      return;
    }

    using mojom::blink::AIManagerCreateClientError;

    switch (error) {
      case AIManagerCreateClientError::kUnableToCreateSession:
      case AIManagerCreateClientError::kUnableToCalculateTokenSize: {
        this->GetResolver()->RejectWithDOMException(
            DOMExceptionCode::kInvalidStateError,
            kExceptionMessageUnableToCreateSession);
        break;
      }
      case AIManagerCreateClientError::kInitialInputTooLarge: {
        CHECK(quota_error_info);
        QuotaExceededError::Reject(
            this->GetResolver(), kExceptionMessageInputTooLarge,
            static_cast<double>(quota_error_info->quota),
            static_cast<double>(quota_error_info->requested));
        break;
      }
      case AIManagerCreateClientError::kUnsupportedLanguage: {
        this->GetResolver()->RejectWithDOMException(
            DOMExceptionCode::kNotSupportedError,
            kExceptionMessageUnsupportedLanguages);
        break;
      }
    }
  }

  // AIContextObserver:
  void ResetReceiver() override { receiver_.reset(); }

 protected:
  // Runs Create* and CanCreate* for the session type; defined in template
  // specializations.
  void RemoteCreate(mojo::PendingRemote<AIMojoCreateClient> client_remote);
  void RemoteCanCreate(CanCreateCallback callback);

  Member<CreateOptions> options_;

 private:
  void Create(mojom::blink::ModelAvailabilityCheckResult result) {
    if (!this->GetResolver()) {
      return;
    }

    auto availability = ConvertModelAvailabilityCheckResult(result);
    if (availability == Availability::kUnavailable) {
      this->GetResolver()->RejectWithDOMException(
          DOMExceptionCode::kNotAllowedError,
          ConvertModelAvailabilityCheckResultToDebugString(result));
      return;
    }

    LocalDOMWindow* window = LocalDOMWindow::From(this->GetScriptState());

    // Writing Assistance APIs are only available within window and extension
    // worker contexts by default. User activation is not consumed by workers,
    // as they lack the ability to do so.
    if (window && RequiresUserActivation(availability) &&
        !MeetsUserActivationRequirements(window)) {
      this->GetResolver()->RejectWithDOMException(
          DOMExceptionCode::kNotAllowedError,
          kExceptionMessageUserActivationRequired);
      return;
    }

    mojo::PendingRemote<AIMojoCreateClient> client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   task_runner_);
    RemoteCreate(std::move(client_remote));
  }

  HeapMojoReceiver<AIMojoCreateClient, AIWritingAssistanceCreateClient>
      receiver_;
  Member<CreateMonitor> monitor_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_WRITING_ASSISTANCE_CREATE_CLIENT_H_
