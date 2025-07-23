// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ANDROID_BACKEND_IMPL_ANDROID_H_
#define SERVICES_ON_DEVICE_MODEL_ANDROID_BACKEND_IMPL_ANDROID_H_

#include "services/on_device_model/backend.h"

namespace on_device_model {

// Android implementation of Backend. Since the base model is not directly
// managed by Chrome, only `CreateWithResult` is implemented.
class BackendImplAndroid : public Backend {
 public:
  BackendImplAndroid();
  ~BackendImplAndroid() override;

  // Backend:
  base::expected<void, ServiceDisconnectReason> CanCreate() override;
  Capabilities GetCapabilities(ModelFile model_file) override;
  base::expected<std::unique_ptr<BackendModel>, mojom::LoadModelResult>
  CreateWithResult(mojom::LoadModelParamsPtr params,
                   base::OnceClosure on_complete) override;
  void LoadTextSafetyModel(
      mojom::TextSafetyModelParamsPtr params,
      mojo::PendingReceiver<mojom::TextSafetyModel> model) override;
  mojom::DevicePerformanceInfoPtr GetDevicePerformanceInfo() override;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_ANDROID_BACKEND_IMPL_ANDROID_H_
