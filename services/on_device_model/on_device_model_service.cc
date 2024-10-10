// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/on_device_model_service.h"

#include <cstdint>
#include <memory>
#include <queue>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "base/uuid.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/on_device_model/fake/on_device_model_fake.h"
#include "services/on_device_model/ml/gpu_blocklist.h"
#include "services/on_device_model/ml/on_device_model_executor.h"
#include "services/on_device_model/ml/on_device_model_internal.h"
#include "services/on_device_model/ml/performance_class.h"
#include "services/on_device_model/ml/ts_model.h"
#include "services/on_device_model/public/cpp/features.h"

namespace on_device_model {
namespace {

class ModelWrapper;

class SessionWrapper final : public mojom::Session {
 public:
  SessionWrapper(base::WeakPtr<ModelWrapper> model,
                 mojo::PendingReceiver<mojom::Session> receiver,
                 std::unique_ptr<ml::SessionImpl> session)
      : model_(model),
        receiver_(this, std::move(receiver)),
        session_(std::move(session)) {}
  ~SessionWrapper() override = default;

  SessionWrapper(const SessionWrapper&) = delete;
  SessionWrapper& operator=(const SessionWrapper&) = delete;

  void AddContext(mojom::InputOptionsPtr input,
                  mojo::PendingRemote<mojom::ContextClient> client) override;
  void Execute(
      mojom::InputOptionsPtr input,
      mojo::PendingRemote<mojom::StreamingResponder> response) override;
  void GetSizeInTokens(mojom::InputPtr input,
                       GetSizeInTokensCallback callback) override;
  void GetSizeInTokensDeprecated(const std::string& text,
                       GetSizeInTokensCallback callback) override;
  void Score(const std::string& text, ScoreCallback callback) override;
  void Clone(mojo::PendingReceiver<mojom::Session> session) override;

  mojo::Receiver<mojom::Session>& receiver() { return receiver_; }

  void AddPreviousContext(mojom::InputOptionsPtr input) {
    previous_contexts_.push_back(std::move(input));
  }

 private:
  void AddContextInternal(mojom::InputOptionsPtr input,
                          mojo::PendingRemote<mojom::ContextClient> client,
                          base::OnceClosure on_complete) {
    session_->AddContext(std::move(input), std::move(client),
                         std::move(on_complete));
  }

  void ExecuteInternal(mojom::InputOptionsPtr input,
                       mojo::PendingRemote<mojom::StreamingResponder> response,
                       base::OnceClosure on_complete) {
    session_->Execute(std::move(input), std::move(response),
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

  void CloneInternal(mojo::PendingReceiver<mojom::Session> session);

  base::WeakPtr<ModelWrapper> model_;
  mojo::Receiver<mojom::Session> receiver_;
  std::unique_ptr<ml::SessionImpl> session_;
  std::vector<mojom::InputOptionsPtr> previous_contexts_;
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
      base::WeakPtr<SessionWrapper> session = nullptr) {
    base::ScopedClosureRunner task_finished(base::BindOnce(
        &ModelWrapper::TaskFinished, weak_ptr_factory_.GetWeakPtr()));
    pending_tasks_.push(PendingTask{
        .session = session,
        .task = base::BindOnce(std::move(task),
                               base::BindOnce([](base::ScopedClosureRunner) {},
                                              std::move(task_finished))),
    });
    RunTaskIfPossible();
  }

  void StartSession(mojo::PendingReceiver<mojom::Session> session) override {
    AddSession(std::move(session),
               model_->CreateSession(receivers_.current_context().get()), {});
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
        base::IgnoreArgs<base::OnceClosure>(std::move(load_adaptation)));
  }

  void AddSession(
      mojo::PendingReceiver<mojom::Session> receiver,
      std::unique_ptr<ml::SessionImpl> session,
      const std::vector<mojom::InputOptionsPtr>& previous_contexts) {
    auto current_session = std::make_unique<SessionWrapper>(
        weak_ptr_factory_.GetWeakPtr(), std::move(receiver),
        std::move(session));
    for (const auto& context : previous_contexts) {
      current_session->AddPreviousContext(context.Clone());
    }
    SessionWrapper* current_session_ptr = current_session.get();
    sessions_.insert(std::move(current_session));
    current_session_ptr->receiver().set_disconnect_handler(
        base::BindOnce(&ModelWrapper::SessionDisconnected,
                       base::Unretained(this), current_session_ptr));
  }

 private:
  struct PendingTask {
    base::WeakPtr<SessionWrapper> session;
    base::OnceClosure task;
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
    auto start = base::TimeTicks::Now();
    auto result = model_->LoadAdaptation(
        std::move(params),
        base::BindOnce(
            [](base::TimeTicks start) {
              base::UmaHistogramMediumTimes(
                  "OnDeviceModel.LoadAdaptationModelDuration",
                  base::TimeTicks::Now() - start);
            },
            start));
    if (!result.has_value()) {
      std::move(callback).Run(result.error());
      return;
    }
    receivers_.Add(this, std::move(model), std::move(*result));
    std::move(callback).Run(mojom::LoadModelResult::kSuccess);
  }

  void RunTaskIfPossible() {
    if (is_running_) {
      return;
    }

    if (pending_tasks_.empty()) {
      return;
    }

    PendingTask pending_task = std::move(pending_tasks_.front());
    pending_tasks_.pop();

    is_running_ = true;
    running_session_ = pending_task.session;
    std::move(pending_task.task).Run();
  }

  void TaskFinished() {
    last_session_ = running_session_;
    is_running_ = false;
    RunTaskIfPossible();
  }

  std::set<std::unique_ptr<SessionWrapper>, base::UniquePtrComparator>
      sessions_;
  std::unique_ptr<ml::OnDeviceModelExecutor> model_;
  mojo::ReceiverSet<
      mojom::OnDeviceModel,
      std::unique_ptr<ml::OnDeviceModelExecutor::ScopedAdaptation>>
      receivers_;
  base::OnceCallback<void(base::WeakPtr<mojom::OnDeviceModel>)> on_delete_;
  std::queue<PendingTask> pending_tasks_;
  bool is_running_ = false;
  base::WeakPtr<SessionWrapper> running_session_;
  // Last session a task was executed in.
  base::WeakPtr<SessionWrapper> last_session_;
  base::WeakPtrFactory<ModelWrapper> weak_ptr_factory_{this};
};

void SessionWrapper::AddContext(
    mojom::InputOptionsPtr input,
    mojo::PendingRemote<mojom::ContextClient> client) {
  if (!model_) {
    return;
  }

  base::OnceClosure save_context =
      base::BindOnce(&SessionWrapper::AddPreviousContext,
                     weak_ptr_factory_.GetWeakPtr(), input.Clone());

  auto add_context_internal = base::BindOnce(
      &SessionWrapper::AddContextInternal, weak_ptr_factory_.GetWeakPtr(),
      std::move(input), std::move(client));

  auto add_context = base::BindOnce(
      [](decltype(add_context_internal) add_context_internal,
         base::OnceClosure save_context, base::OnceClosure finish_callback) {
        std::move(add_context_internal)
            .Run(std::move(save_context).Then(std::move(finish_callback)));
      },
      std::move(add_context_internal), std::move(save_context));

  model_->AddAndRunPendingTask(std::move(add_context),
                               weak_ptr_factory_.GetWeakPtr());
}

void SessionWrapper::Execute(
    mojom::InputOptionsPtr input,
    mojo::PendingRemote<mojom::StreamingResponder> response) {
  if (!model_) {
    return;
  }

  auto execute_internal = base::BindOnce(&SessionWrapper::ExecuteInternal,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         std::move(input), std::move(response));

  model_->AddAndRunPendingTask(std::move(execute_internal),
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

void SessionWrapper::GetSizeInTokensDeprecated(const std::string& text,
                                     GetSizeInTokensCallback callback) {
  auto input = mojom::Input::New();
  input->pieces.push_back(text);
  GetSizeInTokens(std::move(input), std::move(callback));
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

  model_->AddSession(std::move(session), session_->Clone(), previous_contexts_);
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

class LoadFailedService : public mojom::OnDeviceModelService {
 public:
  explicit LoadFailedService(
      mojo::PendingReceiver<mojom::OnDeviceModelService> receiver)
      : receiver_(this, std::move(receiver)) {}

  // mojom::OnDeviceModelService:
  void LoadModel(mojom::LoadModelParamsPtr params,
                 mojo::PendingReceiver<mojom::OnDeviceModel> model,
                 LoadModelCallback callback) override {
    std::move(callback).Run(
        on_device_model::mojom::LoadModelResult::kFailedToLoadLibrary);
  }
  void GetEstimatedPerformanceClass(
      GetEstimatedPerformanceClassCallback callback) override {
    std::move(callback).Run(
        on_device_model::mojom::PerformanceClass::kFailedToLoadLibrary);
  }
  void LoadTextSafetyModel(
      mojom::TextSafetyModelParamsPtr params,
      mojo::PendingReceiver<mojom::TextSafetyModel> model) override {
    model.ResetWithReason(
        static_cast<uint32_t>(
            on_device_model::mojom::LoadModelResult::kFailedToLoadLibrary),
        "Unable to load required shared library.");
  }

 private:
  mojo::Receiver<mojom::OnDeviceModelService> receiver_;
};

class GpuBlockedService : public mojom::OnDeviceModelService {
 public:
  explicit GpuBlockedService(
      mojo::PendingReceiver<mojom::OnDeviceModelService> receiver)
      : receiver_(this, std::move(receiver)) {}

  // mojom::OnDeviceModelService:
  void LoadModel(mojom::LoadModelParamsPtr params,
                 mojo::PendingReceiver<mojom::OnDeviceModel> model,
                 LoadModelCallback callback) override {
    std::move(callback).Run(
        on_device_model::mojom::LoadModelResult::kGpuBlocked);
  }
  void GetEstimatedPerformanceClass(
      GetEstimatedPerformanceClassCallback callback) override {
    std::move(callback).Run(
        on_device_model::mojom::PerformanceClass::kGpuBlocked);
  }
  void LoadTextSafetyModel(
      mojom::TextSafetyModelParamsPtr params,
      mojo::PendingReceiver<mojom::TextSafetyModel> model) override {
    model.ResetWithReason(
        static_cast<uint32_t>(
            on_device_model::mojom::LoadModelResult::kGpuBlocked),
        "GPU is blocklisted.");
  }

 private:
  mojo::Receiver<mojom::OnDeviceModelService> receiver_;
};

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
  // Check for errors and return dummy services.
  // These should probably just receiver.ResetWithReason, but callers
  // are currently expecting these errors to resolve later.
  if (!chrome_ml) {
    return std::make_unique<LoadFailedService>(std::move(receiver));
  }
  if (ml::IsGpuBlocked(chrome_ml->api())) {
    return std::make_unique<GpuBlockedService>(std::move(receiver));
  }
  // No errors, return real service.
  return std::make_unique<OnDeviceModelService>(std::move(receiver),
                                                *chrome_ml);
}

void OnDeviceModelService::LoadModel(
    mojom::LoadModelParamsPtr params,
    mojo::PendingReceiver<mojom::OnDeviceModel> model,
    LoadModelCallback callback) {
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

void OnDeviceModelService::GetEstimatedPerformanceClass(
    GetEstimatedPerformanceClassCallback callback) {
  base::ElapsedTimer timer;
  std::move(callback).Run(ml::GetEstimatedPerformanceClass(*chrome_ml_));
  base::UmaHistogramTimes("OnDeviceModel.BenchmarkDuration", timer.Elapsed());
}

void OnDeviceModelService::LoadTextSafetyModel(
    on_device_model::mojom::TextSafetyModelParamsPtr params,
    mojo::PendingReceiver<mojom::TextSafetyModel> model) {
  ts_holder_.AsyncCall(&ml::TsHolder::Reset)
      .WithArgs(std::move(params), std::move(model));
}

void OnDeviceModelService::DeleteModel(
    base::WeakPtr<mojom::OnDeviceModel> model) {
  if (!model) {
    return;
  }
  auto it = models_.find(model.get());
  CHECK(it != models_.end(), base::NotFatalUntil::M130);
  models_.erase(it);
}

}  // namespace on_device_model
