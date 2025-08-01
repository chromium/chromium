// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/on_device_model_service.h"

#include <cstdint>
#include <list>
#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected_macros.h"
#include "base/uuid.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/on_device_model/backend.h"
#include "services/on_device_model/fake/on_device_model_fake.h"
#include "services/on_device_model/ml/on_device_model_executor.h"
#include "services/on_device_model/public/cpp/features.h"
#include "services/on_device_model/public/cpp/service_client.h"

namespace on_device_model {
namespace {

constexpr char kIdleDisconnectReason[] = "Disconnected due to idle timeout.";

const base::FeatureParam<bool> kForceFastestInference{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_model_force_fastest_inference", false};

// The amount of time a session can remain inactive before the model unloads.
const base::FeatureParam<base::TimeDelta> kModelIdleTimeout{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_model_active_session_idle_timeout", kDefaultModelIdleTimeout};

class ModelWrapper;
class AsrStreamWrapper;

class SessionWrapper final : public mojom::Session {
 public:
  SessionWrapper(base::WeakPtr<ModelWrapper> model,
                 mojo::PendingReceiver<mojom::Session> receiver,
                 std::unique_ptr<BackendSession> session,
                 mojom::Priority priority)
      : model_(model),
        receiver_(this, std::move(receiver)),
        session_(std::move(session)),
        priority_(priority) {}
  ~SessionWrapper() override = default;

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

  base::WeakPtr<ModelWrapper> model_;
  mojo::Receiver<mojom::Session> receiver_;
  std::unique_ptr<BackendSession> session_;
  mojom::Priority priority_;
  std::unique_ptr<AsrStreamWrapper> asr_session_;
  base::WeakPtrFactory<SessionWrapper> weak_ptr_factory_{this};
};

class ModelWrapper final : public mojom::OnDeviceModel {
 public:
  explicit ModelWrapper(
      std::unique_ptr<BackendModel> model,
      mojo::PendingReceiver<mojom::OnDeviceModel> receiver,
      base::OnceCallback<void(base::WeakPtr<mojom::OnDeviceModel>)> on_delete)
      : model_(std::move(model)), on_delete_(std::move(on_delete)) {
    receivers_.Add(this, std::move(receiver),
                   std::unique_ptr<BackendModel::ScopedAdaptation>());
    receivers_.set_disconnect_handler(base::BindRepeating(
        &ModelWrapper::ModelDisconnected, weak_ptr_factory_.GetWeakPtr()));
    RestartIdleTimer();
  }
  ~ModelWrapper() override = default;

  ModelWrapper(const ModelWrapper&) = delete;
  ModelWrapper& operator=(const ModelWrapper&) = delete;

  void AddAndRunPendingTask(
      base::OnceCallback<void(base::OnceClosure finish_callback)> task,
      base::WeakPtr<SessionWrapper> session) {
    base::ScopedClosureRunner task_finished(
        base::BindPostTaskToCurrentDefault(base::BindOnce(
            &ModelWrapper::TaskFinished, weak_ptr_factory_.GetWeakPtr())));
    pending_tasks_.push_back(PendingTask{
        .session = session,
        .task = base::BindOnce(std::move(task),
                               base::BindOnce([](base::ScopedClosureRunner) {},
                                              std::move(task_finished))),
    });
    RunTaskIfPossible();
  }

  void StartSession(mojo::PendingReceiver<mojom::Session> session,
                    mojom::SessionParamsPtr params) override {
    // If the idle timer is active (no ongoing request), restart the timer.
    if (idle_timer_) {
      RestartIdleTimer();
    }
    AddSession(std::move(session),
               model_->CreateSession(receivers_.current_context().get(),
                                     std::move(params)),
               mojom::Priority::kForeground);
  }

  void ClassifyTextSafety(const std::string& text,
                          ClassifyTextSafetyCallback callback) override {
    NOTREACHED();
  }

  void DetectLanguage(const std::string& text,
                      DetectLanguageCallback callback) override {
    NOTREACHED();
  }

  void LoadAdaptation(mojom::LoadAdaptationParamsPtr params,
                      mojo::PendingReceiver<mojom::OnDeviceModel> model,
                      LoadAdaptationCallback callback) override {
    auto load_adaptation = base::BindOnce(
        &ModelWrapper::LoadAdaptationInternal, weak_ptr_factory_.GetWeakPtr(),
        std::move(params), std::move(model), std::move(callback));
    AddAndRunPendingTask(
        base::IgnoreArgs<base::OnceClosure>(std::move(load_adaptation)),
        /*session=*/nullptr);
  }

  void AddSession(mojo::PendingReceiver<mojom::Session> receiver,
                  std::unique_ptr<BackendSession> session,
                  mojom::Priority priority) {
    auto current_session = std::make_unique<SessionWrapper>(
        weak_ptr_factory_.GetWeakPtr(), std::move(receiver), std::move(session),
        priority);
    SessionWrapper* current_session_ptr = current_session.get();
    sessions_.insert(std::move(current_session));
    current_session_ptr->receiver().set_disconnect_handler(
        base::BindOnce(&ModelWrapper::SessionDisconnected,
                       base::Unretained(this), current_session_ptr));
  }

  void SetForceQueueingForTesting(bool force_queueing) {
    force_queueing_for_testing_ = force_queueing;
    if (!force_queueing) {
      RunTaskIfPossible();
    }
  }

 private:
  struct PendingTask {
    base::WeakPtr<SessionWrapper> session;
    base::OnceClosure task;
    base::TimeTicks start = base::TimeTicks::Now();
  };

  void SessionDisconnected(SessionWrapper* ptr) {
    auto it = sessions_.find(ptr);
    if (it != sessions_.end()) {
      sessions_.erase(it);
    }
  }

  void ModelDisconnected() {
    if (receivers_.empty()) {
      std::move(on_delete_).Run(weak_ptr_factory_.GetWeakPtr());
    }
  }

  void LoadAdaptationInternal(mojom::LoadAdaptationParamsPtr params,
                              mojo::PendingReceiver<mojom::OnDeviceModel> model,
                              LoadAdaptationCallback callback) {
    receivers_.Add(this, std::move(model),
                   model_->LoadAdaptation(std::move(params)));
    std::move(callback).Run(mojom::LoadModelResult::kSuccess);
  }

  void RunTaskIfPossible() {
    if (is_running_ || force_queueing_for_testing_) {
      return;
    }

    if (pending_tasks_.empty()) {
      // If the queue is empty, make sure the idle timer is running.
      RestartIdleTimer();
      return;
    }

    std::optional<PendingTask> pending_task;
    // First try to take any foreground tasks.
    for (auto it = pending_tasks_.begin(); it != pending_tasks_.end(); ++it) {
      SessionWrapper* session = it->session.get();
      if (!session || session->IsForeground()) {
        pending_task = std::move(*it);
        pending_tasks_.erase(it);
        break;
      }
    }
    // If no foreground task is available, take what's left.
    const bool is_foreground = pending_task.has_value();
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

  void TaskFinished() {
    is_running_ = false;
    RunTaskIfPossible();
  }

  void RestartIdleTimer() {
    idle_timer_.emplace();
    idle_timer_->Start(
        FROM_HERE, kModelIdleTimeout.Get(),
        base::BindOnce(&ModelWrapper::OnIdleTimeout, base::Unretained(this)));
  }

  void OnIdleTimeout() {
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

  std::unique_ptr<BackendModel> model_;
  std::set<std::unique_ptr<SessionWrapper>, base::UniquePtrComparator>
      sessions_;
  mojo::ReceiverSet<mojom::OnDeviceModel,
                    std::unique_ptr<BackendModel::ScopedAdaptation>>
      receivers_;
  base::OnceCallback<void(base::WeakPtr<mojom::OnDeviceModel>)> on_delete_;
  std::list<PendingTask> pending_tasks_;
  bool is_running_ = false;
  bool force_queueing_for_testing_ = false;

  // This timer is active if there are no pending tasks. If the timer triggers,
  // the model remote will be reset.
  std::optional<base::OneShotTimer> idle_timer_;

  base::WeakPtrFactory<ModelWrapper> weak_ptr_factory_{this};
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

std::unique_ptr<Backend> DefaultImpl() {
  if (base::FeatureList::IsEnabled(features::kUseFakeChromeML)) {
    return std::make_unique<ml::BackendImpl>(fake_ml::GetFakeChromeML());
  }
#if defined(ENABLE_ML_INTERNAL)
  return std::make_unique<ml::BackendImpl>(::ml::ChromeML::Get());
#else
  return std::make_unique<ml::BackendImpl>(fake_ml::GetFakeChromeML());
#endif  // defined(ENABLE_ML_INTERNAL)
}

}  // namespace

OnDeviceModelService::OnDeviceModelService(
    mojo::PendingReceiver<mojom::OnDeviceModelService> receiver,
    const ml::ChromeML& chrome_ml)
    : receiver_(this, std::move(receiver)),
      backend_(std::make_unique<ml::BackendImpl>(&chrome_ml)) {}

OnDeviceModelService::OnDeviceModelService(
    mojo::PendingReceiver<mojom::OnDeviceModelService> receiver,
    std::unique_ptr<Backend> backend)
    : receiver_(this, std::move(receiver)), backend_(std::move(backend)) {}

OnDeviceModelService::~OnDeviceModelService() = default;

// static
std::unique_ptr<mojom::OnDeviceModelService> OnDeviceModelService::Create(
    mojo::PendingReceiver<mojom::OnDeviceModelService> receiver,
    std::unique_ptr<Backend> backend) {
  if (!backend) {
    backend = DefaultImpl();
  }
  RETURN_IF_ERROR(backend->CanCreate(),
                  [&](ServiceDisconnectReason reason)
                      -> std::unique_ptr<mojom::OnDeviceModelService> {
                    receiver.ResetWithReason(static_cast<uint32_t>(reason),
                                             "Error loading backend.");
                    return nullptr;
                  });
  // No errors, return real service.
  return std::make_unique<OnDeviceModelService>(std::move(receiver),
                                                std::move(backend));
}

void OnDeviceModelService::LoadModel(
    mojom::LoadModelParamsPtr params,
    mojo::PendingReceiver<mojom::OnDeviceModel> model,
    LoadModelCallback callback) {
  if (kForceFastestInference.Get()) {
    params->performance_hint = ml::ModelPerformanceHint::kFastestInference;
  }
  auto start = base::TimeTicks::Now();
  auto model_impl = backend_->CreateWithResult(
      std::move(params),
      base::BindOnce(
          [](base::TimeTicks start) {
            base::UmaHistogramMediumTimes("OnDeviceModel.LoadModelDuration",
                                          base::TimeTicks::Now() - start);
          },
          start));
  if (!model_impl.has_value()) {
    std::move(callback).Run(model_impl.error());
    return;
  }
  models_.insert(std::make_unique<ModelWrapper>(
      std::move(model_impl.value()), std::move(model),
      base::BindOnce(&OnDeviceModelService::DeleteModel,
                     base::Unretained(this))));
  std::move(callback).Run(mojom::LoadModelResult::kSuccess);
}

void OnDeviceModelService::GetCapabilities(ModelFile model_file,
                                           GetCapabilitiesCallback callback) {
  std::move(callback).Run(backend_->GetCapabilities(std::move(model_file)));
}

void OnDeviceModelService::GetDevicePerformanceInfo(
    GetDevicePerformanceInfoCallback callback) {
#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, we explicitly allowlist only Chromebook Plus devices,
  // so skip the benchmark and return a fixed performance profile.
  auto perf_info = on_device_model::mojom::DevicePerformanceInfo::New();
  // Fix the performance to 'High', which should allow all Nano models to run.
  perf_info->performance_class = on_device_model::mojom::PerformanceClass::kHigh;
  // Chromebook+ devices have 8GB RAM+, so half of that can be VRAM.
  perf_info->vram_mb = 4096;
  std::move(callback).Run(std::move(perf_info));
#else
  // This is expected to take awhile in some cases, so run on a background
  // thread to avoid blocking the main thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          [](OnDeviceModelService* service) {
            if (!service) {
              return on_device_model::mojom::DevicePerformanceInfo::New();
            }
            base::ElapsedTimer timer;
            on_device_model::mojom::DevicePerformanceInfoPtr perf_info =
                service->backend_->GetDevicePerformanceInfo();
            base::UmaHistogramTimes("OnDeviceModel.BenchmarkDuration",
                                    timer.Elapsed());
            return perf_info;
          },
          // WeakPtr won't work here because they're not thread-safe.
          // Raw pointers are ok because OnDeviceModelService will always live
          // as long as the ODML process does, so if this code is running the
          // service must be alive.
          base::Unretained(this)),
      std::move(callback));
#endif
}

void OnDeviceModelService::LoadTextSafetyModel(
    on_device_model::mojom::TextSafetyModelParamsPtr params,
    mojo::PendingReceiver<mojom::TextSafetyModel> model) {
  backend_->LoadTextSafetyModel(std::move(params), std::move(model));
}

void OnDeviceModelService::SetForceQueueingForTesting(bool force_queueing) {
  for (auto& model : models_) {
    static_cast<ModelWrapper*>(model.get())
        ->SetForceQueueingForTesting(force_queueing);  // IN-TEST
  }
}

void OnDeviceModelService::DeleteModel(
    base::WeakPtr<mojom::OnDeviceModel> model) {
  if (!model) {
    return;
  }
  auto it = models_.find(model.get());
  CHECK(it != models_.end());
  models_.erase(it);
}

}  // namespace on_device_model
