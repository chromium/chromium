// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_SERVICE_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_SERVICE_IMPL_H_

#include <memory>

#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_keepalive.h"
#include "services/video_capture/device_factory_provider_impl.h"
#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"
#include "services/video_capture/public/mojom/testing_controls.mojom.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace video_capture {

class ServiceImpl : public service_manager::Service,
                    public service_manager::ServiceKeepalive::TimeoutObserver {
 public:
  // If |shutdown_delay| is provided, the service will shut itself down as soon
  // as no client was connect for the corresponding duration.
  explicit ServiceImpl(base::Optional<base::TimeDelta> shutdown_delay);
  ~ServiceImpl() override;

  static std::unique_ptr<service_manager::Service> Create();

  void SetDestructionObserver(base::OnceClosure observer_cb);
  void SetFactoryProviderClientConnectedObserver(
      base::RepeatingClosure observer_cb);
  void SetFactoryProviderClientDisconnectedObserver(
      base::RepeatingClosure observer_cb);
  void SetShutdownTimeoutCancelledObserver(base::RepeatingClosure observer_cb);
  bool HasNoContextRefs();

  // service_manager::Service implementation.
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;
  bool OnServiceManagerConnectionLost() override;

  // service_manager::ServiceKeepalive::TimeoutObserver implementation.
  void OnTimeoutExpired() override;
  void OnTimeoutCancelled() override;

 private:
  void OnDeviceFactoryProviderRequest(
      mojom::DeviceFactoryProviderRequest request);
  void OnTestingControlsRequest(mojom::TestingControlsRequest request);
  void MaybeRequestQuitDelayed();
  void MaybeRequestQuit();
  void LazyInitializeDeviceFactoryProvider();
  void OnProviderClientDisconnected();

  const base::Optional<base::TimeDelta> shutdown_delay_;
#if defined(OS_WIN)
  // COM must be initialized in order to access the video capture devices.
  base::win::ScopedCOMInitializer com_initializer_;
#endif
  service_manager::BinderRegistry registry_;
  mojo::BindingSet<mojom::DeviceFactoryProvider> factory_provider_bindings_;
  std::unique_ptr<DeviceFactoryProviderImpl> device_factory_provider_;
  std::unique_ptr<service_manager::ServiceKeepalive> ref_factory_;

  // Callbacks that can optionally be set by clients.
  base::OnceClosure destruction_cb_;
  base::RepeatingClosure factory_provider_client_connected_cb_;
  base::RepeatingClosure factory_provider_client_disconnected_cb_;
  base::RepeatingClosure shutdown_timeout_cancelled_cb_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<ServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceImpl);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_SERVICE_IMPL_H_
