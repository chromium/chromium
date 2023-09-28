// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/on_device_model_service.h"

#include "services/on_device_model/on_device_model.h"

namespace on_device_model {

OnDeviceModelService::OnDeviceModelService(
    mojo::PendingReceiver<mojom::OnDeviceModelService> receiver)
    : receiver_(this, std::move(receiver)) {}

OnDeviceModelService::~OnDeviceModelService() = default;

void OnDeviceModelService::LoadModel(mojom::LoadModelParamsPtr params,
                                     LoadModelCallback callback) {
  mojo::PendingRemote<mojom::OnDeviceModel> remote;
  model_receivers_.Add(std::make_unique<OnDeviceModel>(std::move(params)),
                       remote.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(mojom::LoadModelResult::NewModel(std::move(remote)));
}

}  // namespace on_device_model
