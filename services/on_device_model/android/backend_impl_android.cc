// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/android/backend_impl_android.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "base/types/expected.h"
#include "services/on_device_model/android/backend_model_impl_android.h"
#include "services/on_device_model/public/cpp/service_client.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace on_device_model {

BackendImplAndroid::BackendImplAndroid() = default;

BackendImplAndroid::~BackendImplAndroid() = default;

base::expected<void, ServiceDisconnectReason> BackendImplAndroid::CanCreate() {
  return base::ok();
}

Capabilities BackendImplAndroid::GetCapabilities(ModelFile model_file) {
  NOTIMPLEMENTED();
  return {};
}

base::expected<std::unique_ptr<BackendModel>, mojom::LoadModelResult>
BackendImplAndroid::CreateWithResult(mojom::LoadModelParamsPtr params,
                                     base::OnceClosure on_complete) {
  auto model = std::make_unique<BackendModelImplAndroid>();
  // `on_complete` record metrics, so it is called after the model is created to
  // include the time to create the model.
  std::move(on_complete).Run();
  return base::ok(std::move(model));
}

void BackendImplAndroid::LoadTextSafetyModel(
    mojom::TextSafetyModelParamsPtr params,
    mojo::PendingReceiver<mojom::TextSafetyModel> model) {
  NOTIMPLEMENTED();
}

mojom::DevicePerformanceInfoPtr BackendImplAndroid::GetDevicePerformanceInfo() {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace on_device_model
