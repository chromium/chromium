// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_PROVIDER_H_
#define SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_PROVIDER_H_

#include <memory>

#include "base/threading/thread.h"
#include "media/capture/video/video_capture_jpeg_decoder.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"

namespace video_capture {

class VirtualDeviceEnabledDeviceFactory;

class DeviceFactoryProviderImpl : public mojom::DeviceFactoryProvider {
 public:
  DeviceFactoryProviderImpl();
  ~DeviceFactoryProviderImpl() override;

  void SetServiceRef(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);

  // mojom::DeviceFactoryProvider implementation.
  void InjectGpuDependencies(
      mojom::AcceleratorFactoryPtr accelerator_factory) override;
  void ConnectToDeviceFactory(mojom::DeviceFactoryRequest request) override;

 private:
  class GpuDependenciesContext;

  void LazyInitializeGpuDependenciesContext();
  void LazyInitializeDeviceFactory();
  void OnFactoryClientDisconnected();

  mojo::BindingSet<mojom::DeviceFactory> factory_bindings_;
  std::unique_ptr<VirtualDeviceEnabledDeviceFactory> device_factory_;
  std::unique_ptr<service_manager::ServiceContextRef> service_ref_;
  std::unique_ptr<GpuDependenciesContext> gpu_dependencies_context_;

  DISALLOW_COPY_AND_ASSIGN(DeviceFactoryProviderImpl);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_PROVIDER_H_
