// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_INTERNAL_H_
#define SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_INTERNAL_H_

#include "base/component_export.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/gpu_blocklist.h"
#include "services/on_device_model/public/cpp/on_device_model.h"

namespace ml {

class COMPONENT_EXPORT(ON_DEVICE_MODEL_ML) OnDeviceModelInternalImpl final
    : public on_device_model::OnDeviceModelShim {
 public:
  explicit OnDeviceModelInternalImpl(const ChromeML* chrome_ml,
                                     GpuBlocklist gpu_blocklist);
  ~OnDeviceModelInternalImpl() override;

  base::expected<std::unique_ptr<on_device_model::OnDeviceModel>,
                 on_device_model::mojom::LoadModelResult>
  CreateModel(on_device_model::mojom::LoadModelParamsPtr params,
              base::OnceClosure on_complete) const override;

  on_device_model::mojom::PerformanceClass GetEstimatedPerformanceClass()
      const override;

  const raw_ptr<const ChromeML> chrome_ml_;
  GpuBlocklist gpu_blocklist_;
};

COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
const on_device_model::OnDeviceModelShim* GetOnDeviceModelInternalImpl();

COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
const on_device_model::OnDeviceModelShim*
GetOnDeviceModelInternalImplWithoutGpuBlocklistForTesting();

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_INTERNAL_H_
