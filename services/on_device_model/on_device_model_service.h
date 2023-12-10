// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ON_DEVICE_MODEL_SERVICE_H_
#define SERVICES_ON_DEVICE_MODEL_ON_DEVICE_MODEL_SERVICE_H_

#include "base/component_export.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
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
  void GetEstimatedPerformanceClass(
      GetEstimatedPerformanceClassCallback callback) override;

 private:
  static base::expected<std::unique_ptr<OnDeviceModel>, mojom::LoadModelResult>
  CreateModel(mojom::LoadModelParamsPtr params);

  mojo::Receiver<mojom::OnDeviceModelService> receiver_;
  mojo::UniqueReceiverSet<mojom::OnDeviceModel> model_receivers_;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_ON_DEVICE_MODEL_SERVICE_H_
