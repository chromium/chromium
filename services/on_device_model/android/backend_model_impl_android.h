// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ANDROID_BACKEND_MODEL_IMPL_ANDROID_H_
#define SERVICES_ON_DEVICE_MODEL_ANDROID_BACKEND_MODEL_IMPL_ANDROID_H_

#include "components/optimization_guide/proto/model_execution.pb.h"
#include "services/on_device_model/backend_model.h"

namespace on_device_model {

// Uses the OnDeviceModel APIs that are available on Android to create sessions.
// Each feature should create its own backend model object.
class BackendModelImplAndroid : public BackendModel {
 public:
  explicit BackendModelImplAndroid(
      optimization_guide::proto::ModelExecutionFeature feature);
  ~BackendModelImplAndroid() override;

  // BackendModel:
  std::unique_ptr<BackendSession> CreateSession(
      const ScopedAdaptation* adaptation,
      on_device_model::mojom::SessionParamsPtr params) override;
  std::unique_ptr<ScopedAdaptation> LoadAdaptation(
      on_device_model::mojom::LoadAdaptationParamsPtr params) override;
  void UnloadAdaptation(uint32_t adaptation_id) override;

 private:
  optimization_guide::proto::ModelExecutionFeature feature_;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_ANDROID_BACKEND_MODEL_IMPL_ANDROID_H_
