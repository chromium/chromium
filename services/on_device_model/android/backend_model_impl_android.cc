// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/android/backend_model_impl_android.h"

#include <memory>

#include "base/notimplemented.h"
#include "services/on_device_model/android/backend_session_impl_android.h"

namespace on_device_model {

BackendModelImplAndroid::BackendModelImplAndroid(
    optimization_guide::proto::ModelExecutionFeature feature)
    : feature_(feature) {}

BackendModelImplAndroid::~BackendModelImplAndroid() = default;

std::unique_ptr<BackendSession> BackendModelImplAndroid::CreateSession(
    const ScopedAdaptation* adaptation,
    on_device_model::mojom::SessionParamsPtr params) {
  return std::make_unique<BackendSessionImplAndroid>(feature_,
                                                     std::move(params));
}

std::unique_ptr<BackendModel::ScopedAdaptation>
BackendModelImplAndroid::LoadAdaptation(
    on_device_model::mojom::LoadAdaptationParamsPtr params) {
  NOTIMPLEMENTED();
  return nullptr;
}

void BackendModelImplAndroid::UnloadAdaptation(uint32_t adaptation_id) {
  NOTIMPLEMENTED();
}

}  // namespace on_device_model
