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
#include "base/uuid.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/on_device_model/fake/on_device_model_fake.h"
#include "services/on_device_model/ml/gpu_blocklist.h"
#include "services/on_device_model/ml/on_device_model_executor.h"
#include "services/on_device_model/ml/on_device_model_internal.h"
#include "services/on_device_model/ml/performance_class.h"
#include "services/on_device_model/ml/ts_model.h"
#include "services/on_device_model/public/cpp/features.h"
#include "services/on_device_model/public/cpp/service_client.h"

namespace on_device_model {
namespace {

const base::FeatureParam<bool> kForceFastestInference{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_model_force_fastest_inference", false};

class ModelWrapper;

class SessionWrapper final : public mojom::Session {
 public:
  SessionWrapper(base::WeakPtr<ModelWrapper> model,
                 mojo::PendingReceiver<mojom::Session> receiver,
                 std::unique_ptr<ml::SessionImpl> session,
                 mojom::Priority priority)
      : model_(model),
        receiver_(this, std::move(receiver)),
        session_(std::move(session)),
        priority_(priority) {}
  ~SessionWrapper() override = default;

  SessionWrapper(const SessionWrapper&) = delete;
  SessionWrapper& operator=(const SessionWrapper&) = delete;

  void Append(mojom::AppendOptionsPtr options,
              mojo::PendingRemote<mojom::ContextClient> client) override;
  void Generate(
      mojom::GenerateOptionsPtr options,
      mojo::PendingRemote<mojom::StreamingResponder> response) override;
  void GetSizeInTokens(mojom::InputPtr input,
                       GetSizeInTokensCallback callback) override;
  void Score(const std::string& text, ScoreCallback callback) override;
  void GetProbabilitiesBlocking(
      const std::string& text,
      GetProbabilitiesBlockingCallback callback) override;
  void Clone(mojo::PendingReceiver<mojom::Session> session) override;
  void SetPriority(mojom::Priority priority) override { priority_ = priority; }

  mojo::Receiver<mojom::Session>& receiver() { return receiver_; }

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

  base::WeakPtr<ModelWrapper> model_;
  mojo::Receiver<mojom::Session> receiver_;
  std::unique_ptr<ml::SessionImpl> session_;
  mojom::Priority priority_;
  base::WeakPtrFactory<SessionWrapper> weak_ptr_factory_{this};
};

class ModelWrapper final : public mojom::OnDeviceModel {
 public:
  explicit ModelWrapper(
      std::unique_ptr<ml::OnDeviceModelExecutor> model,
      mojo::PendingReceiver<mojom::OnDeviceModel> receiver,
      base::OnceCallback<void(base::WeakPtr<mojom::OnDeviceModel>)> on_delete)
      : model_(std::move(model)), on_delete_(std::move(on_delete)) {
    receivers_.Add(
        this, std::move(receiver),
        std::unique_ptr<ml::OnDeviceModelExecutor::ScopedAdaptation>());
    receivers_.set_disconnect_handler(base::BindRepeating(
        &ModelWrapper::ModelDisconnected, weak_ptr_factory_.GetWeakPtr()));
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
                  std::unique_ptr<ml::SessionImpl> session,
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
    std::move(pending_task->task).Run();
  }

  void TaskFinished() {
    is_running_ = false;
    RunTaskIfPossible();
  }

  std::unique_ptr<ml::OnDeviceModelExecutor> model_;
  std::set<std::unique_ptr<SessionWrapper>, base::UniquePtrComparator>
      sessions_;
  mojo::ReceiverSet<
      mojom::OnDeviceModel,
      std::unique_ptr<ml::OnDeviceModelExecutor::ScopedAdaptation>>
      receivers_;
  base::OnceCallback<void(base::WeakPtr<mojom::OnDeviceModel>)> on_delete_;
  std::list<PendingTask> pending_tasks_;
  bool is_running_ = false;
  bool force_queueing_for_testing_ = false;
  base::WeakPtrFactory<ModelWrapper> weak_ptr_factory_{this};
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
    mojo::PendingRemote<mojom::StreamingResponder> response) {
  if (!model_) {
    return;
  }

  auto generate_internal = base::BindOnce(
      &SessionWrapper::GenerateInternal, weak_ptr_factory_.GetWeakPtr(),
      std::move(options), std::move(response));

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

void SessionWrapper::CloneInternal(
    mojo::PendingReceiver<mojom::Session> session) {
  if (!model_) {
    return;
  }

  model_->AddSession(std::move(session), session_->Clone(), priority_);
}

const ml::ChromeML* DefaultImpl() {
  if (base::FeatureList::IsEnabled(features::kUseFakeChromeML)) {
    return fake_ml::GetFakeChromeML();
  }
#if defined(ENABLE_ML_INTERNAL)
  return ::ml::ChromeML::Get();
#else
  return fake_ml::GetFakeChromeML();
#endif
}

}  // namespace

OnDeviceModelService::OnDeviceModelService(
    mojo::PendingReceiver<mojom::OnDeviceModelService> receiver,
    const ml::OnDeviceModelInternalImpl* impl)
    : OnDeviceModelService(std::move(receiver), *impl->chrome_ml()) {}

OnDeviceModelService::OnDeviceModelService(
    mojo::PendingReceiver<mojom::OnDeviceModelService> receiver,
    const ml::ChromeML& chrome_ml)
    : receiver_(this, std::move(receiver)),
      chrome_ml_(chrome_ml),
      ts_holder_(ml::TsHolder::Create(chrome_ml_)) {}
OnDeviceModelService::~OnDeviceModelService() = default;

std::unique_ptr<mojom::OnDeviceModelService> OnDeviceModelService::Create(
    mojo::PendingReceiver<mojom::OnDeviceModelService> receiver) {
  const ml::ChromeML* chrome_ml = DefaultImpl();
  if (!chrome_ml) {
    receiver.ResetWithReason(
        static_cast<uint32_t>(ServiceDisconnectReason::kFailedToLoadLibrary),
        "Unable to load chrome_ml library.");
    return nullptr;
  }
  if (!optimization_guide::features::ForceCpuBackendForOnDeviceModel() &&
      ml::IsGpuBlocked(chrome_ml->api())) {
    receiver.ResetWithReason(
        static_cast<uint32_t>(ServiceDisconnectReason::kGpuBlocked),
        "The device's GPU is not supported.");
    return nullptr;
  }
  // No errors, return real service.
  return std::make_unique<OnDeviceModelService>(std::move(receiver),
                                                *chrome_ml);
}

void OnDeviceModelService::LoadModel(
    mojom::LoadModelParamsPtr params,
    mojo::PendingReceiver<mojom::OnDeviceModel> model,
    LoadModelCallback callback) {
  if (kForceFastestInference.Get()) {
    params->performance_hint = ml::ModelPerformanceHint::kFastestInference;
  }
  auto start = base::TimeTicks::Now();
  auto model_impl = ml::OnDeviceModelExecutor::CreateWithResult(
      *chrome_ml_, std::move(params),
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
  std::move(callback).Run(ml::OnDeviceModelExecutor::GetCapabilities(
      *chrome_ml_, std::move(model_file)));
}

void OnDeviceModelService::GetEstimatedPerformanceClass(
    GetEstimatedPerformanceClassCallback callback) {
  // This is expected to take awhile in some cases, so run on a background
  // thread to avoid blocking the main thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          [](const ml::ChromeML& chrome_ml) {
            base::ElapsedTimer timer;
            on_device_model::mojom::PerformanceClass perf_class =
                ml::GetEstimatedPerformanceClass(chrome_ml);
            base::UmaHistogramTimes("OnDeviceModel.BenchmarkDuration",
                                    timer.Elapsed());
            return perf_class;
          },
          // base::Unretained is safe since chrome_ml_ refers to a global.
          base::Unretained(chrome_ml_)),
      std::move(callback));
}

void OnDeviceModelService::LoadTextSafetyModel(
    on_device_model::mojom::TextSafetyModelParamsPtr params,
    mojo::PendingReceiver<mojom::TextSafetyModel> model) {
  ts_holder_.AsyncCall(&ml::TsHolder::Reset)
      .WithArgs(std::move(params), std::move(model));
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
