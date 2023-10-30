// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/on_device_model_service.h"

namespace on_device_model {
namespace {

class OnDeviceModel : public mojom::OnDeviceModel {
 public:
  OnDeviceModel() = default;
  ~OnDeviceModel() override = default;

  OnDeviceModel(const OnDeviceModel&) = delete;
  OnDeviceModel& operator=(const OnDeviceModel&) = delete;

  void Execute(
      const std::string& input,
      mojo::PendingRemote<mojom::StreamingResponder> response) override {
    mojo::Remote<mojom::StreamingResponder> remote(std::move(response));
    remote->OnResponse("Input: " + input + "\n");
    remote->OnComplete();
  }
};

}  // namespace

// static
std::unique_ptr<mojom::OnDeviceModel> OnDeviceModelService::CreateModel(
    ModelAssets assets) {
  return std::make_unique<OnDeviceModel>();
}

// static
mojom::PerformanceClass OnDeviceModelService::GetEstimatedPerformanceClass() {
  return mojom::PerformanceClass::kError;
}

}  // namespace on_device_model
