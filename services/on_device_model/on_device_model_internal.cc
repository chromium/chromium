// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/chrome_ml_instance.h"
#include "services/on_device_model/on_device_model_service.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "third_party/ml/public/on_device_model_executor.h"
#include "third_party/ml/public/utils.h"

namespace on_device_model {

namespace {

class OnDeviceModel : public mojom::OnDeviceModel {
 public:
  explicit OnDeviceModel(std::unique_ptr<ml::OnDeviceModelExecutor> executor)
      : executor_(std::move(executor)) {}
  ~OnDeviceModel() override = default;

  OnDeviceModel(const OnDeviceModel&) = delete;
  OnDeviceModel& operator=(const OnDeviceModel&) = delete;

  void Execute(
      const std::string& input,
      mojo::PendingRemote<mojom::StreamingResponder> response) override {
    executor_->Execute(input, std::move(response));
  }

 private:
  std::unique_ptr<ml::OnDeviceModelExecutor> executor_;
};

}  // namespace

// static
std::unique_ptr<mojom::OnDeviceModel> OnDeviceModelService::CreateModel(
    ModelAssets assets) {
  if (!GetChromeMLInstance()) {
    return nullptr;
  }

  auto executor = ml::OnDeviceModelExecutor::Create(*GetChromeMLInstance(),
                                                    std::move(assets));
  if (!executor) {
    return nullptr;
  }
  return std::make_unique<OnDeviceModel>(std::move(executor));
}

// static
mojom::PerformanceClass OnDeviceModelService::GetEstimatedPerformanceClass() {
  if (!GetChromeMLInstance()) {
    return mojom::PerformanceClass::kError;
  }
  return ml::GetEstimatedPerformanceClass(*GetChromeMLInstance());
}

}  // namespace on_device_model
