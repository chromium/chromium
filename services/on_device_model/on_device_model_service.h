// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ON_DEVICE_MODEL_SERVICE_H_
#define SERVICES_ON_DEVICE_MODEL_ON_DEVICE_MODEL_SERVICE_H_

#include <set>

#include "base/component_export.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/on_device_model/platform_model_loader.h"
#include "services/on_device_model/public/cpp/on_device_model.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "sandbox/policy/linux/sandbox_linux.h"
#endif

namespace on_device_model {

class COMPONENT_EXPORT(ON_DEVICE_MODEL) OnDeviceModelService
    : public mojom::OnDeviceModelService {
 public:
  // Must be called in the service's process before sandbox initialization.
  // These are defined separately in pre_sandbox_init.cc for explicit security
  // review coverage.
  [[nodiscard]] static bool PreSandboxInit();

  // Must be called in the service's process after the run loop finished.
  [[nodiscard]] static bool Shutdown();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  static void AddSandboxLinuxOptions(
      sandbox::policy::SandboxLinux::Options& options);
#endif

  static mojom::PerformanceClass GetEstimatedPerformanceClass();

  explicit OnDeviceModelService(
      mojo::PendingReceiver<mojom::OnDeviceModelService> receiver);
  ~OnDeviceModelService() override;

  OnDeviceModelService(const OnDeviceModelService&) = delete;
  OnDeviceModelService& operator=(const OnDeviceModelService&) = delete;

  // mojom::OnDeviceModelService:
  void LoadModel(mojom::LoadModelParamsPtr params,
                 mojo::PendingReceiver<mojom::OnDeviceModel> model,
                 LoadModelCallback callback) override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void LoadPlatformModel(const base::Uuid& uuid,
                         mojo::PendingReceiver<mojom::OnDeviceModel> model,
                         LoadPlatformModelCallback callback) override;
#endif
  void GetEstimatedPerformanceClass(
      GetEstimatedPerformanceClassCallback callback) override;

  size_t NumModelsForTesting() const { return models_.size(); }

 private:
  static base::expected<std::unique_ptr<OnDeviceModel>, mojom::LoadModelResult>
  CreateModel(mojom::LoadModelParamsPtr params);

  void DeleteModel(base::WeakPtr<mojom::OnDeviceModel> model);

  mojo::Receiver<mojom::OnDeviceModelService> receiver_;
  std::set<std::unique_ptr<mojom::OnDeviceModel>, base::UniquePtrComparator>
      models_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<PlatformModelLoader> platform_model_loader_;
#endif
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_ON_DEVICE_MODEL_SERVICE_H_
