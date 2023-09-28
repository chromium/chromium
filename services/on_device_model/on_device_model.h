// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ON_DEVICE_MODEL_H_
#define SERVICES_ON_DEVICE_MODEL_ON_DEVICE_MODEL_H_

#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace on_device_model {

class OnDeviceModel : public mojom::OnDeviceModel {
 public:
  explicit OnDeviceModel(mojom::LoadModelParamsPtr params);
  ~OnDeviceModel() override;

  OnDeviceModel(const OnDeviceModel&) = delete;
  OnDeviceModel& operator=(const OnDeviceModel&) = delete;

  void Execute(
      const std::string& input,
      mojo::PendingRemote<mojom::StreamingResponder> response) override;

 private:
  const mojom::LoadModelParamsPtr params_;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_ON_DEVICE_MODEL_H_
