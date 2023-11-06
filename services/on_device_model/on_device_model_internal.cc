// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/chrome_ml_instance.h"
#include "services/on_device_model/on_device_model_service.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/cpp/on_device_model.h"
#include "third_party/ml/public/on_device_model_executor.h"
#include "third_party/ml/public/utils.h"

namespace on_device_model {

// static
std::unique_ptr<OnDeviceModel> OnDeviceModelService::CreateModel(
    ModelAssets assets) {
  if (!GetChromeMLInstance()) {
    return nullptr;
  }

  auto executor = ml::OnDeviceModelExecutor::Create(*GetChromeMLInstance(),
                                                    std::move(assets));
  if (!executor) {
    return nullptr;
  }
  return executor;
}

// static
mojom::PerformanceClass OnDeviceModelService::GetEstimatedPerformanceClass() {
  if (!GetChromeMLInstance()) {
    return mojom::PerformanceClass::kError;
  }
  return ml::GetEstimatedPerformanceClass(*GetChromeMLInstance());
}

}  // namespace on_device_model
