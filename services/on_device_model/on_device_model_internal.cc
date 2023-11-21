// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "components/optimization_guide/internal/public/on_device_model_executor.h"
#include "components/optimization_guide/internal/public/utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/chrome_ml_instance.h"
#include "services/on_device_model/on_device_model_service.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/cpp/on_device_model.h"

namespace on_device_model {

// static
base::expected<std::unique_ptr<OnDeviceModel>, mojom::LoadModelResult>
OnDeviceModelService::CreateModel(mojom::LoadModelParamsPtr params) {
  if (!GetChromeMLInstance()) {
    return base::unexpected(mojom::LoadModelResult::kFailedToLoadLibrary);
  }

  return ml::OnDeviceModelExecutor::CreateWithResult(*GetChromeMLInstance(),
                                                     std::move(params));
}

// static
mojom::PerformanceClass OnDeviceModelService::GetEstimatedPerformanceClass() {
  if (!GetChromeMLInstance()) {
    return mojom::PerformanceClass::kError;
  }
  return ml::GetEstimatedPerformanceClass(*GetChromeMLInstance());
}

}  // namespace on_device_model
