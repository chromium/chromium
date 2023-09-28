// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/on_device_model.h"

#include "mojo/public/cpp/bindings/remote.h"

namespace on_device_model {

OnDeviceModel::OnDeviceModel(mojom::LoadModelParamsPtr params)
    : params_(std::move(params)) {}

OnDeviceModel::~OnDeviceModel() = default;

void OnDeviceModel::Execute(
    const std::string& input,
    mojo::PendingRemote<mojom::StreamingResponder> response) {
  mojo::Remote<mojom::StreamingResponder> remote(std::move(response));
  // TODO(cduvall): Make this work.
  remote->OnResponse("Model: " + params_->path.MaybeAsASCII());
  remote->OnResponse("Input: " + input);
  remote->OnComplete();
}

}  // namespace on_device_model
