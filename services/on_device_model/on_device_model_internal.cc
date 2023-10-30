// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/chrome_ml_instance.h"
#include "services/on_device_model/on_device_model_service.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/cpp/on_device_model.h"
#include "third_party/ml/public/on_device_model_executor.h"
#include "third_party/ml/public/utils.h"

namespace on_device_model {

namespace {

// TODO(cduvall): Implement sessions in ml::OnDeviceModelExecutor.
class SessionImpl : public OnDeviceModel::Session {
 public:
  explicit SessionImpl(ml::OnDeviceModelExecutor* executor)
      : executor_(executor) {}
  ~SessionImpl() override = default;

  SessionImpl(const SessionImpl&) = delete;
  SessionImpl& operator=(const SessionImpl&) = delete;

  void AddContext(mojom::InputOptionsPtr input) override {}

  void Execute(
      mojom::InputOptionsPtr input,
      mojo::PendingRemote<mojom::StreamingResponder> response) override {
    executor_->Execute(input->text, std::move(response));
  }

 private:
  raw_ptr<ml::OnDeviceModelExecutor> executor_;
};

class OnDeviceModelImpl : public OnDeviceModel {
 public:
  explicit OnDeviceModelImpl(
      std::unique_ptr<ml::OnDeviceModelExecutor> executor)
      : executor_(std::move(executor)) {}
  ~OnDeviceModelImpl() override = default;

  OnDeviceModelImpl(const OnDeviceModelImpl&) = delete;
  OnDeviceModelImpl& operator=(const OnDeviceModelImpl&) = delete;

  std::unique_ptr<Session> CreateSession() override {
    return std::make_unique<SessionImpl>(executor_.get());
  }

 private:
  std::unique_ptr<ml::OnDeviceModelExecutor> executor_;
};

}  // namespace

// static
std::unique_ptr<OnDeviceModel> OnDeviceModelService::CreateModel(
    ModelAssets assets) {
  if (!GetChromeMLInstance()) {
    return nullptr;
  }

  auto executor = ml::OnDeviceModelExecutor::Create(*GetChromeMLInstance(),
                                                    std::move(assets));
  if (!executor) {
    return nullptr;
  }
  return std::make_unique<OnDeviceModelImpl>(std::move(executor));
}

// static
mojom::PerformanceClass OnDeviceModelService::GetEstimatedPerformanceClass() {
  if (!GetChromeMLInstance()) {
    return mojom::PerformanceClass::kError;
  }
  return ml::GetEstimatedPerformanceClass(*GetChromeMLInstance());
}

}  // namespace on_device_model
