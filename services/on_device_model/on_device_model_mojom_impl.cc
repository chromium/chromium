// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/on_device_model_mojom_impl.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/backend.h"
#include "services/on_device_model/public/cpp/features.h"
#include "services/on_device_model/public/cpp/service_client.h"

namespace on_device_model {

namespace {

constexpr char kIdleDisconnectReason[] = "Disconnected due to idle timeout.";

// The amount of time a session can remain inactive before the model unloads.
const base::FeatureParam<base::TimeDelta> kModelIdleTimeout{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_model_active_session_idle_timeout", kDefaultModelIdleTimeout};

class AsrStreamWrapper;

class SessionWrapper final : public mojom::Session {
 public:
  SessionWrapper(base::WeakPtr<OnDeviceModelMojomImpl> model,
                 mojo::PendingReceiver<mojom::Session> receiver,
                 std::unique_ptr<BackendSession> session,
                 mojom::Priority priority);
  ~SessionWrapper() override;

  SessionWrapper(const SessionWrapper&) = delete;
  SessionWrapper& operator=(const SessionWrapper&) = delete;

  // mojom::Session:
  void Append(mojom::AppendOptionsPtr options,
              mojo::PendingRemote<mojom::ContextClient> client) override;
  void Generate(
      mojom::GenerateOptionsPtr options,
      mojo::PendingRemote<mojom::StreamingResponder> responder) override;
  void GetSizeInTokens(mojom::InputPtr input,
                       GetSizeInTokensCallback callback) override;
  void Score(const std::string& text, ScoreCallback callback) override;
  void GetProbabilitiesBlocking(
      const std::string& text,
      GetProbabilitiesBlockingCallback callback) override;
  void Clone(mojo::PendingReceiver<mojom::Session> session) override;
  void SetPriority(mojom::Priority priority) override { priority_ = priority; }

  mojo::Receiver<mojom::Session>& receiver() { return receiver_; }
  BackendSession& backend() { return *session_; }
  void AsrStream(
      mojom::AsrStreamOptionsPtr options,
      mojo::PendingReceiver<mojom::AsrStreamInput> stream,
      mojo::PendingRemote<mojom::AsrStreamResponder> response) override;

  bool IsForeground() const {
    return priority_ == mojom::Priority::kForeground;
  }

 private:
  void AppendInternal(mojom::AppendOptionsPtr options,
                      mojo::PendingRemote<mojom::ContextClient> client,
                      base::OnceClosure on_complete) {
    session_->Append(std::move(options), std::move(client),
                     std::move(on_complete));
  }

  void GenerateInternal(mojom::GenerateOptionsPtr input,
                        mojo::PendingRemote<mojom::StreamingResponder> response,
                        base::OnceClosure on_complete) {
    session_->Generate(std::move(input), std::move(response),
                       std::move(on_complete));
  }

  void GetSizeInTokensInternal(mojom::InputPtr input,
                               GetSizeInTokensCallback callback,
                               base::OnceClosure on_complete) {
    session_->SizeInTokens(std::move(input),
                           std::move(callback).Then(std::move(on_complete)));
  }

  void ScoreInternal(const std::string& text,
                     ScoreCallback callback,
                     base::OnceClosure on_complete) {
    session_->Score(text, std::move(callback).Then(std::move(on_complete)));
  }

  void GetProbabilitiesBlockingInternal(
      const std::string& text,
      GetProbabilitiesBlockingCallback callback,
      base::OnceClosure on_complete) {
    session_->GetProbabilitiesBlocking(
        text, std::move(callback).Then(std::move(on_complete)));
  }

  void CloneInternal(mojo::PendingReceiver<mojom::Session> session);
  void AsrStreamInternal(
      mojom::AsrStreamOptionsPtr options,
      mojo::PendingReceiver<mojom::AsrStreamInput> stream,
      mojo::PendingRemote<mojom::AsrStreamResponder> response,
      base::OnceClosure on_complete);

  base::WeakPtr<OnDeviceModelMojomImpl> model_;
  mojo::Receiver<mojom::Session> receiver_;
  std::unique_ptr<BackendSession> session_;
  mojom::Priority priority_;
  std::unique_ptr<AsrStreamWrapper> asr_session_;
  base::WeakPtrFactory<SessionWrapper> weak_ptr_factory_{this};
};

class AsrStreamWrapper final : public mojom::AsrStreamInput {
 public:
  AsrStreamWrapper(base::WeakPtr<SessionWrapper> session,
                   mojo::PendingReceiver<mojom::AsrStreamInput> receiver)
      : session_(session), receiver_(this, std::move(receiver)) {}
  ~AsrStreamWrapper() override = default;

  AsrStreamWrapper(const AsrStreamWrapper&) = delete;
  AsrStreamWrapper& operator=(const AsrStreamWrapper&) = delete;

  void AddAudioChunk(mojom::AudioDataPtr data) override {
    if (!session_) {
      return;  // Session was already closed.
    }
    session_->backend().AsrAddAudioChunk(std::move(data));
  }

 private:
  base::WeakPtr<SessionWrapper> session_;
  mojo::Receiver<mojom::AsrStreamInput> receiver_;
  base::WeakPtrFactory<AsrStreamWrapper> weak_ptr_factory_{this};
};

SessionWrapper::SessionWrapper(base::WeakPtr<OnDeviceModelMojomImpl> model,
                               mojo::PendingReceiver<mojom::Session> receiver,
                               std::unique_ptr<BackendSession> session,
                               mojom::Priority priority)
    : model_(model),
      receiver_(this, std::move(receiver)),
      session_(std::move(session)),
      priority_(priority) {}

SessionWrapper::~SessionWrapper() = default;

void SessionWrapper::Append(mojom::AppendOptionsPtr options,
                            mojo::PendingRemote<mojom::ContextClient> client) {
  if (!model_) {
    return;
  }
  auto append_internal = base::BindOnce(&SessionWrapper::AppendInternal,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        std::move(options), std::move(client));

  model_->AddAndRunPendingTask(std::move(append_internal),
                               weak_ptr_factory_.GetWeakPtr());
}

void SessionWrapper::Generate(
    mojom::GenerateOptionsPtr options,
    mojo::PendingRemote<mojom::StreamingResponder> responder) {
  if (!model_) {
    return;
  }

  auto generate_internal = base::BindOnce(
      &SessionWrapper::GenerateInternal, weak_ptr_factory_.GetWeakPtr(),
      std::move(options), std::move(responder));

  model_->AddAndRunPendingTask(std::move(generate_internal),
                               weak_ptr_factory_.GetWeakPtr());
}

void SessionWrapper::GetSizeInTokens(mojom::InputPtr input,
                                     GetSizeInTokensCallback callback) {
  if (!model_) {
    return;
  }

  auto size_in_tokens_internal = base::BindOnce(
      &SessionWrapper::GetSizeInTokensInternal, weak_ptr_factory_.GetWeakPtr(),
      std::move(input), std::move(callback));

  model_->AddAndRunPendingTask(std::move(size_in_tokens_internal),
                               weak_ptr_factory_.GetWeakPtr());
}

void SessionWrapper::Score(const std::string& text, ScoreCallback callback) {
  if (!model_) {
    return;
  }

  model_->AddAndRunPendingTask(
      base::BindOnce(&SessionWrapper::ScoreInternal,
                     weak_ptr_factory_.GetWeakPtr(), text, std::move(callback)),
      weak_ptr_factory_.GetWeakPtr());
}

void SessionWrapper::GetProbabilitiesBlocking(
    const std::string& text,
    GetProbabilitiesBlockingCallback callback) {
  if (!model_) {
    std::move(callback).Run(std::vector<float>());
    return;
  }

  model_->AddAndRunPendingTask(
      base::BindOnce(&SessionWrapper::GetProbabilitiesBlockingInternal,
                     weak_ptr_factory_.GetWeakPtr(), text, std::move(callback)),
      weak_ptr_factory_.GetWeakPtr());
}

void SessionWrapper::Clone(mojo::PendingReceiver<mojom::Session> session) {
  if (!model_) {
    return;
  }

  model_->AddAndRunPendingTask(
      base::IgnoreArgs<base::OnceClosure>(
          base::BindOnce(&SessionWrapper::CloneInternal,
                         weak_ptr_factory_.GetWeakPtr(), std::move(session))),
      weak_ptr_factory_.GetWeakPtr());
}

void SessionWrapper::AsrStream(
    mojom::AsrStreamOptionsPtr options,
    mojo::PendingReceiver<mojom::AsrStreamInput> stream,
    mojo::PendingRemote<mojom::AsrStreamResponder> response) {
  if (!model_) {
    return;
  }
  model_->AddAndRunPendingTask(
      base::BindOnce(&SessionWrapper::AsrStreamInternal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(options),
                     std::move(stream), std::move(response)),
      weak_ptr_factory_.GetWeakPtr());
}

void SessionWrapper::CloneInternal(
    mojo::PendingReceiver<mojom::Session> session) {
  if (!model_) {
    return;
  }

  model_->AddSession(std::move(session), session_->Clone(), priority_);
}

void SessionWrapper::AsrStreamInternal(
    mojom::AsrStreamOptionsPtr options,
    mojo::PendingReceiver<mojom::AsrStreamInput> stream,
    mojo::PendingRemote<mojom::AsrStreamResponder> response,
    base::OnceClosure on_complete) {
  if (!model_) {
    return;
  }
  DCHECK_EQ(asr_session_, nullptr);
  auto speech_stream_wrapper = std::make_unique<AsrStreamWrapper>(
      weak_ptr_factory_.GetWeakPtr(), std::move(stream));
  asr_session_ = std::move(speech_stream_wrapper);
  session_->AsrStream(std::move(options), std::move(response));
}

}  // namespace

struct OnDeviceModelMojomImpl::PendingTask {
  base::WeakPtr<SessionWrapper> session;
  base::OnceClosure task;
  base::TimeTicks start = base::TimeTicks::Now();
};

OnDeviceModelMojomImpl::OnDeviceModelMojomImpl(
    std::unique_ptr<BackendModel> model,
    mojo::PendingReceiver<mojom::OnDeviceModel> receiver,
    base::OnceCallback<void(base::WeakPtr<mojom::OnDeviceModel>)> on_delete)
    : model_(std::move(model)), on_delete_(std::move(on_delete)) {
  receivers_.Add(this, std::move(receiver),
                 std::unique_ptr<BackendModel::ScopedAdaptation>());
  receivers_.set_disconnect_handler(
      base::BindRepeating(&OnDeviceModelMojomImpl::ModelDisconnected,
                          weak_ptr_factory_.GetWeakPtr()));
  RestartIdleTimer();
}

OnDeviceModelMojomImpl::~OnDeviceModelMojomImpl() = default;

void OnDeviceModelMojomImpl::AddAndRunPendingTask(
    base::OnceCallback<void(base::OnceClosure finish_callback)> task,
    base::WeakPtr<SessionWrapper> session) {
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelMojomImpl::AddAndRunPendingTask");
  base::ScopedClosureRunner task_finished(base::BindPostTaskToCurrentDefault(
      base::BindOnce(&OnDeviceModelMojomImpl::TaskFinished,
                     weak_ptr_factory_.GetWeakPtr())));
  pending_tasks_.push_back(base::WrapUnique(new PendingTask{
      .session = session,
      .task = base::BindOnce(std::move(task),
                             base::BindOnce([](base::ScopedClosureRunner) {},
                                            std::move(task_finished))),
  }));
  RunTaskIfPossible();
}

void OnDeviceModelMojomImpl::StartSession(
    mojo::PendingReceiver<mojom::Session> session,
    mojom::SessionParamsPtr params) {
  TRACE_EVENT("optimization_guide", "OnDeviceModelMojomImpl::StartSession");
  // If the idle timer is active (no ongoing request), restart the timer.
  if (idle_timer_) {
    RestartIdleTimer();
  }
  AddSession(std::move(session),
             model_->CreateSession(receivers_.current_context().get(),
                                   std::move(params)),
             mojom::Priority::kForeground);
}

void OnDeviceModelMojomImpl::ClassifyTextSafety(
    const std::string& text,
    ClassifyTextSafetyCallback callback) {
  NOTREACHED();
}

void OnDeviceModelMojomImpl::DetectLanguage(const std::string& text,
                                            DetectLanguageCallback callback) {
  NOTREACHED();
}

void OnDeviceModelMojomImpl::LoadAdaptation(
    mojom::LoadAdaptationParamsPtr params,
    mojo::PendingReceiver<mojom::OnDeviceModel> model,
    LoadAdaptationCallback callback) {
  TRACE_EVENT("optimization_guide", "OnDeviceModelMojomImpl::LoadAdaptation");
  auto load_adaptation =
      base::BindOnce(&OnDeviceModelMojomImpl::LoadAdaptationInternal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(params),
                     std::move(model), std::move(callback));
  AddAndRunPendingTask(
      base::IgnoreArgs<base::OnceClosure>(std::move(load_adaptation)),
      /*session=*/nullptr);
}

void OnDeviceModelMojomImpl::AddSession(
    mojo::PendingReceiver<mojom::Session> receiver,
    std::unique_ptr<BackendSession> session,
    mojom::Priority priority) {
  TRACE_EVENT("optimization_guide", "OnDeviceModelMojomImpl::AddSession");
  auto current_session = std::make_unique<SessionWrapper>(
      weak_ptr_factory_.GetWeakPtr(), std::move(receiver), std::move(session),
      priority);
  SessionWrapper* current_session_ptr = current_session.get();
  sessions_.insert(std::move(current_session));
  current_session_ptr->receiver().set_disconnect_handler(
      base::BindOnce(&OnDeviceModelMojomImpl::SessionDisconnected,
                     base::Unretained(this), current_session_ptr));
}

void OnDeviceModelMojomImpl::SetForceQueueingForTesting(bool force_queueing) {
  force_queueing_for_testing_ = force_queueing;
  if (!force_queueing) {
    RunTaskIfPossible();
  }
}

void OnDeviceModelMojomImpl::SessionDisconnected(SessionWrapper* ptr) {
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelMojomImpl::SessionDisconnected");
  auto it = sessions_.find(ptr);
  if (it != sessions_.end()) {
    sessions_.erase(it);
  }
}

void OnDeviceModelMojomImpl::ModelDisconnected() {
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelMojomImpl::ModelDisconnected");
  if (receivers_.empty()) {
    std::move(on_delete_).Run(weak_ptr_factory_.GetWeakPtr());
  }
}

void OnDeviceModelMojomImpl::LoadAdaptationInternal(
    mojom::LoadAdaptationParamsPtr params,
    mojo::PendingReceiver<mojom::OnDeviceModel> model,
    LoadAdaptationCallback callback) {
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelMojomImpl::LoadAdaptationInternal");
  receivers_.Add(this, std::move(model),
                 model_->LoadAdaptation(std::move(params)));
  std::move(callback).Run(mojom::LoadModelResult::kSuccess);
}

void OnDeviceModelMojomImpl::RunTaskIfPossible() {
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelMojomImpl::RunTaskIfPossible");
  if (is_running_ || force_queueing_for_testing_) {
    return;
  }

  if (pending_tasks_.empty()) {
    // If the queue is empty, make sure the idle timer is running.
    RestartIdleTimer();
    return;
  }

  std::unique_ptr<PendingTask> pending_task;
  // First try to take any foreground tasks.
  for (auto it = pending_tasks_.begin(); it != pending_tasks_.end(); ++it) {
    SessionWrapper* session = (*it)->session.get();
    if (!session || session->IsForeground()) {
      pending_task = std::move(*it);
      pending_tasks_.erase(it);
      break;
    }
  }
  // If no foreground task is available, take what's left.
  const bool is_foreground = (pending_task != nullptr);
  if (!pending_task) {
    pending_task = std::move(pending_tasks_.front());
    pending_tasks_.pop_front();
  }

  base::UmaHistogramMediumTimes(
      base::StrCat({"OnDeviceModel.QueueTime.",
                    is_foreground ? "Foreground" : "Background"}),
      base::TimeTicks::Now() - pending_task->start);

  is_running_ = true;
  idle_timer_ = std::nullopt;
  std::move(pending_task->task).Run();
}

void OnDeviceModelMojomImpl::TaskFinished() {
  TRACE_EVENT("optimization_guide", "OnDeviceModelMojomImpl::TaskFinished");
  is_running_ = false;
  RunTaskIfPossible();
}

void OnDeviceModelMojomImpl::RestartIdleTimer() {
  TRACE_EVENT("optimization_guide", "OnDeviceModelMojomImpl::RestartIdleTimer");
  idle_timer_.emplace();
  idle_timer_->Start(FROM_HERE, kModelIdleTimeout.Get(),
                     base::BindOnce(&OnDeviceModelMojomImpl::OnIdleTimeout,
                                    base::Unretained(this)));
}

void OnDeviceModelMojomImpl::OnIdleTimeout() {
  TRACE_EVENT("optimization_guide", "OnDeviceModelMojomImpl::OnIdleTimeout");
  for (auto& session : sessions_) {
    session->receiver().ResetWithReason(
        static_cast<uint32_t>(ModelDisconnectReason::kIdleShutdown),
        kIdleDisconnectReason);
  }
  sessions_.clear();
  receivers_.ClearWithReason(
      static_cast<uint32_t>(ModelDisconnectReason::kIdleShutdown),
      kIdleDisconnectReason);
  ModelDisconnected();
}

}  // namespace on_device_model
