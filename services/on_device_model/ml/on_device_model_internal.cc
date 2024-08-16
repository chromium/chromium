// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/no_destructor.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/gpu_blocklist.h"
#include "services/on_device_model/ml/on_device_model_executor.h"
#include "services/on_device_model/ml/utils.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/cpp/on_device_model.h"

namespace ml {

namespace {

class OnDeviceModelInternalImpl final
    : public on_device_model::OnDeviceModelShim {
 public:
  explicit OnDeviceModelInternalImpl(const ChromeML* chrome_ml,
                                     GpuBlocklist gpu_blocklist);
  ~OnDeviceModelInternalImpl() override;

  base::expected<std::unique_ptr<on_device_model::OnDeviceModel>,
                 on_device_model::mojom::LoadModelResult>
  CreateModel(on_device_model::mojom::LoadModelParamsPtr params,
              base::OnceClosure on_complete) const override {
    if (!chrome_ml_) {
      return base::unexpected(
          on_device_model::mojom::LoadModelResult::kFailedToLoadLibrary);
    }
    if (gpu_blocklist_.IsGpuBlocked(chrome_ml_->api())) {
      return base::unexpected(
          on_device_model::mojom::LoadModelResult::kGpuBlocked);
    }

    return ml::OnDeviceModelExecutor::CreateWithResult(
        *chrome_ml_, std::move(params), std::move(on_complete));
  }

  on_device_model::mojom::PerformanceClass GetEstimatedPerformanceClass()
      const override {
    if (!chrome_ml_) {
      return on_device_model::mojom::PerformanceClass::kFailedToLoadLibrary;
    }
    if (gpu_blocklist_.IsGpuBlocked(chrome_ml_->api())) {
      return on_device_model::mojom::PerformanceClass::kGpuBlocked;
    }
    return ml::GetEstimatedPerformanceClass(*chrome_ml_);
  }

  const raw_ptr<const ChromeML> chrome_ml_;
  GpuBlocklist gpu_blocklist_;
};

OnDeviceModelInternalImpl::OnDeviceModelInternalImpl(const ChromeML* chrome_ml,
                                                     GpuBlocklist gpu_blocklist)
    : chrome_ml_(chrome_ml), gpu_blocklist_(gpu_blocklist) {}

OnDeviceModelInternalImpl::~OnDeviceModelInternalImpl() = default;

}  // namespace

COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
const on_device_model::OnDeviceModelShim* GetOnDeviceModelInternalImpl() {
  static const base::NoDestructor<OnDeviceModelInternalImpl> impl(
      ::ml::ChromeML::Get(), GpuBlocklist{});
  return impl.get();
}

COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
const on_device_model::OnDeviceModelShim*
GetOnDeviceModelInternalImplWithoutGpuBlocklistForTesting() {
  static const base::NoDestructor<OnDeviceModelInternalImpl> impl(
      ::ml::ChromeML::Get(), GpuBlocklist{
                                 .skip_for_testing = true,
                             });
  return impl.get();
}

}  // namespace ml
