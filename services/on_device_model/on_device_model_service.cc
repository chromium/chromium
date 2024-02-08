// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/on_device_model_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/on_device_model/public/cpp/on_device_model.h"

namespace on_device_model {
namespace {

class SessionWrapper : public mojom::Session {
 public:
  SessionWrapper(mojo::PendingReceiver<mojom::Session> receiver,
                 std::unique_ptr<OnDeviceModel::Session> session)
      : receiver_(this, std::move(receiver)), session_(std::move(session)) {}
  ~SessionWrapper() override = default;

  SessionWrapper(const SessionWrapper&) = delete;
  SessionWrapper& operator=(const SessionWrapper&) = delete;

  void AddContext(mojom::InputOptionsPtr input,
                  mojo::PendingRemote<mojom::ContextClient> client) override {
    session_->AddContext(std::move(input), std::move(client));
  }

  void Execute(
      mojom::InputOptionsPtr input,
      mojo::PendingRemote<mojom::StreamingResponder> response) override {
    session_->Execute(std::move(input), std::move(response));
  }

  mojo::Receiver<mojom::Session>& receiver() { return receiver_; }

 private:
  mojo::Receiver<mojom::Session> receiver_;
  std::unique_ptr<OnDeviceModel::Session> session_;
};

class ModelWrapper : public mojom::OnDeviceModel {
 public:
  explicit ModelWrapper(
      std::unique_ptr<on_device_model::OnDeviceModel> model,
      mojo::PendingReceiver<mojom::OnDeviceModel> receiver,
      base::OnceCallback<void(mojom::OnDeviceModel*)> on_delete)
      : model_(std::move(model)), on_delete_(std::move(on_delete)) {
    receivers_.Add(this, std::move(receiver), std::nullopt);
    receivers_.set_disconnect_handler(base::BindRepeating(
        &ModelWrapper::ModelDisconnected, base::Unretained(this)));
  }
  ~ModelWrapper() override = default;

  ModelWrapper(const ModelWrapper&) = delete;
  ModelWrapper& operator=(const ModelWrapper&) = delete;

  void StartSession(mojo::PendingReceiver<mojom::Session> session) override {
    current_session_ = std::make_unique<SessionWrapper>(
        std::move(session),
        model_->CreateSession(receivers_.current_context()));
    current_session_->receiver().set_disconnect_handler(base::BindOnce(
        &ModelWrapper::SessionDisconnected, base::Unretained(this)));
  }

  void LoadAdaptation(mojom::LoadAdaptationParamsPtr params,
                      mojo::PendingReceiver<mojom::OnDeviceModel> model,
                      LoadAdaptationCallback callback) override {
    current_session_.reset();
    auto result = model_->LoadAdaptation(std::move(params));
    if (!result.has_value()) {
      std::move(callback).Run(result.error());
      return;
    }
    receivers_.Add(this, std::move(model), *result);
    std::move(callback).Run(mojom::LoadModelResult::kSuccess);
  }

 private:
  void SessionDisconnected() { current_session_.reset(); }

  void ModelDisconnected() {
    if (receivers_.empty()) {
      std::move(on_delete_).Run(this);
    }
  }

  std::unique_ptr<SessionWrapper> current_session_;
  std::unique_ptr<on_device_model::OnDeviceModel> model_;
  mojo::ReceiverSet<mojom::OnDeviceModel, std::optional<uint32_t>> receivers_;
  base::OnceCallback<void(mojom::OnDeviceModel*)> on_delete_;
};

}  // namespace

OnDeviceModelService::OnDeviceModelService(
    mojo::PendingReceiver<mojom::OnDeviceModelService> receiver)
    : receiver_(this, std::move(receiver)) {}

OnDeviceModelService::~OnDeviceModelService() = default;

void OnDeviceModelService::LoadModel(
    mojom::LoadModelParamsPtr params,
    mojo::PendingReceiver<mojom::OnDeviceModel> model,
    LoadModelCallback callback) {
  base::ElapsedTimer timer;
  auto model_impl = CreateModel(std::move(params));
  if (!model_impl.has_value()) {
    std::move(callback).Run(model_impl.error());
    return;
  }

  base::UmaHistogramMediumTimes("OnDeviceModel.LoadModelDuration",
                                timer.Elapsed());
  models_.insert(std::make_unique<ModelWrapper>(
      std::move(model_impl.value()), std::move(model),
      base::BindOnce(&OnDeviceModelService::DeleteModel,
                     base::Unretained(this))));
  std::move(callback).Run(mojom::LoadModelResult::kSuccess);
}

void OnDeviceModelService::GetEstimatedPerformanceClass(
    GetEstimatedPerformanceClassCallback callback) {
  base::ElapsedTimer timer;
  std::move(callback).Run(GetEstimatedPerformanceClass());
  base::UmaHistogramTimes("OnDeviceModel.BenchmarkDuration", timer.Elapsed());
}

void OnDeviceModelService::DeleteModel(mojom::OnDeviceModel* model) {
  auto it = models_.find(model);
  DCHECK(it != models_.end());
  models_.erase(it);
}

}  // namespace on_device_model
