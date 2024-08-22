// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_INTERNAL_H_
#define SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_INTERNAL_H_

#include "base/component_export.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/gpu_blocklist.h"
#include "services/on_device_model/ml/on_device_model_executor.h"

namespace ml {

class COMPONENT_EXPORT(ON_DEVICE_MODEL_ML) OnDeviceModelInternalImpl final {
 public:
  explicit OnDeviceModelInternalImpl(const ChromeML* chrome_ml,
                                     GpuBlocklist gpu_blocklist);
  ~OnDeviceModelInternalImpl();

  base::expected<std::unique_ptr<OnDeviceModelExecutor>,
                 on_device_model::mojom::LoadModelResult>
  CreateModel(on_device_model::mojom::LoadModelParamsPtr params,
              base::OnceClosure on_complete) const;

  on_device_model::mojom::PerformanceClass GetEstimatedPerformanceClass() const;

  const raw_ptr<const ChromeML> chrome_ml_;
  GpuBlocklist gpu_blocklist_;
};

COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
const OnDeviceModelInternalImpl* GetOnDeviceModelInternalImpl();

COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
const OnDeviceModelInternalImpl*
GetOnDeviceModelInternalImplWithoutGpuBlocklistForTesting();

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_INTERNAL_H_
