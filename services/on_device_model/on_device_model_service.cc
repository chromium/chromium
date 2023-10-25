// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/on_device_model_service.h"

namespace on_device_model {

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
  model_receivers_.Add(std::move(model),
                       remote.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(mojom::LoadModelResult::NewModel(std::move(remote)));
}

void OnDeviceModelService::GetEstimatedPerformanceClass(
    GetEstimatedPerformanceClassCallback callback) {
  std::move(callback).Run(GetEstimatedPerformanceClass());
}

}  // namespace on_device_model
