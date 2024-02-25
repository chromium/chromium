// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/on_device_model_executor.h"
#include "services/on_device_model/ml/utils.h"
#include "services/on_device_model/on_device_model_service.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/cpp/on_device_model.h"

namespace on_device_model {

// static
base::expected<std::unique_ptr<OnDeviceModel>, mojom::LoadModelResult>
OnDeviceModelService::CreateModel(mojom::LoadModelParamsPtr params) {
  auto* chrome_ml = ml::ChromeML::Get();
  if (!chrome_ml) {
    return base::unexpected(mojom::LoadModelResult::kFailedToLoadLibrary);
  }

  return ml::OnDeviceModelExecutor::CreateWithResult(*chrome_ml,
                                                     std::move(params));
}

// static
mojom::PerformanceClass OnDeviceModelService::GetEstimatedPerformanceClass() {
  auto* chrome_ml = ml::ChromeML::Get();
  if (!chrome_ml) {
    return mojom::PerformanceClass::kFailedToLoadLibrary;
  }
  if (chrome_ml->IsGpuBlocked()) {
    return mojom::PerformanceClass::kGpuBlocked;
  }
  return ml::GetEstimatedPerformanceClass(*chrome_ml);
}

}  // namespace on_device_model
