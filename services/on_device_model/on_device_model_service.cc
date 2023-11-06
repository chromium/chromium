// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/on_device_model_service.h"

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

  void AddContext(mojom::InputOptionsPtr input) override {
    session_->AddContext(std::move(input));
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
  explicit ModelWrapper(std::unique_ptr<on_device_model::OnDeviceModel> model)
      : model_(std::move(model)) {}
  ~ModelWrapper() override = default;

  ModelWrapper(const ModelWrapper&) = delete;
  ModelWrapper& operator=(const ModelWrapper&) = delete;

  void StartSession(mojo::PendingReceiver<mojom::Session> session) override {
    current_session_ = std::make_unique<SessionWrapper>(
        std::move(session), model_->CreateSession());
    current_session_->receiver().set_disconnect_handler(base::BindOnce(
        &ModelWrapper::SessionDisconnected, base::Unretained(this)));
  }

 private:
  void SessionDisconnected() { current_session_.reset(); }

  std::unique_ptr<SessionWrapper> current_session_;
  std::unique_ptr<on_device_model::OnDeviceModel> model_;
};

}  // namespace

OnDeviceModelService::OnDeviceModelService(
    mojo::PendingReceiver<mojom::OnDeviceModelService> receiver)
    : receiver_(this, std::move(receiver)) {}

OnDeviceModelService::~OnDeviceModelService() = default;

void OnDeviceModelService::LoadModel(ModelAssets assets,
                                     LoadModelCallback callback) {
  auto model = CreateModel(std::move(assets));
  if (!model) {
    std::move(callback).Run(
        mojom::LoadModelResult::NewError("Failed to create model."));
    return;
  }

  mojo::PendingRemote<mojom::OnDeviceModel> remote;
  model_receivers_.Add(std::make_unique<ModelWrapper>(std::move(model)),
                       remote.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(mojom::LoadModelResult::NewModel(std::move(remote)));
}

void OnDeviceModelService::GetEstimatedPerformanceClass(
    GetEstimatedPerformanceClassCallback callback) {
  std::move(callback).Run(GetEstimatedPerformanceClass());
}

}  // namespace on_device_model
