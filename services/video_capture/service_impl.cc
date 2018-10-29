// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/service_impl.h"

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/video_capture/device_factory_provider_impl.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/public/uma/video_capture_service_event.h"
#include "services/video_capture/testing_controls_impl.h"

namespace video_capture {

ServiceImpl::ServiceImpl(base::Optional<base::TimeDelta> shutdown_delay)
    : shutdown_delay_(shutdown_delay), weak_factory_(this) {}

ServiceImpl::~ServiceImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (destruction_cb_)
    std::move(destruction_cb_).Run();
}

// static
std::unique_ptr<service_manager::Service> ServiceImpl::Create() {
#if defined(OS_ANDROID)
  // On Android, we do not use automatic service shutdown, because when shutting
  // down the service, we lose caching of the supported formats, and re-querying
  // these can take several seconds on certain Android devices.
  return std::make_unique<ServiceImpl>(base::Optional<base::TimeDelta>());
#else
  return std::make_unique<ServiceImpl>(base::TimeDelta::FromSeconds(5));
#endif
}

void ServiceImpl::SetDestructionObserver(base::OnceClosure observer_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  destruction_cb_ = std::move(observer_cb);
}

void ServiceImpl::SetFactoryProviderClientConnectedObserver(
    base::RepeatingClosure observer_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  factory_provider_client_connected_cb_ = std::move(observer_cb);
}

void ServiceImpl::SetFactoryProviderClientDisconnectedObserver(
    base::RepeatingClosure observer_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  factory_provider_client_disconnected_cb_ = std::move(observer_cb);
}

void ServiceImpl::SetShutdownTimeoutCancelledObserver(
    base::RepeatingClosure observer_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  shutdown_timeout_cancelled_cb_ = std::move(observer_cb);
}

bool ServiceImpl::HasNoContextRefs() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return ref_factory_->HasNoRefs();
}

void ServiceImpl::OnStart() {
  DCHECK(thread_checker_.CalledOnValidThread());

  video_capture::uma::LogVideoCaptureServiceEvent(
      video_capture::uma::SERVICE_STARTED);

  // Do not replace |ref_factory_| if one has already been set via
  // SetServiceContextRefProviderForTesting().
  if (!ref_factory_) {
    ref_factory_ = std::make_unique<service_manager::ServiceKeepalive>(
        context(), shutdown_delay_, this);
  }

  registry_.AddInterface<mojom::DeviceFactoryProvider>(
      // Unretained |this| is safe because |registry_| is owned by |this|.
      base::Bind(&ServiceImpl::OnDeviceFactoryProviderRequest,
                 base::Unretained(this)));
  registry_.AddInterface<mojom::TestingControls>(
      // Unretained |this| is safe because |registry_| is owned by |this|.
      base::Bind(&ServiceImpl::OnTestingControlsRequest,
                 base::Unretained(this)));

  // Unretained |this| is safe because |factory_provider_bindings_| is owned by
  // |this|.
  factory_provider_bindings_.set_connection_error_handler(base::BindRepeating(
      &ServiceImpl::OnProviderClientDisconnected, base::Unretained(this)));
}

void ServiceImpl::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  DCHECK(thread_checker_.CalledOnValidThread());
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

bool ServiceImpl::OnServiceManagerConnectionLost() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

void ServiceImpl::OnTimeoutExpired() {
  video_capture::uma::LogVideoCaptureServiceEvent(
      video_capture::uma::SERVICE_SHUTTING_DOWN_BECAUSE_NO_CLIENT);
}

void ServiceImpl::OnTimeoutCancelled() {
  video_capture::uma::LogVideoCaptureServiceEvent(
      video_capture::uma::SERVICE_SHUTDOWN_TIMEOUT_CANCELED);
  if (shutdown_timeout_cancelled_cb_)
    shutdown_timeout_cancelled_cb_.Run();
}

void ServiceImpl::OnDeviceFactoryProviderRequest(
    mojom::DeviceFactoryProviderRequest request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LazyInitializeDeviceFactoryProvider();
  if (factory_provider_bindings_.empty())
    device_factory_provider_->SetServiceRef(ref_factory_->CreateRef());
  factory_provider_bindings_.AddBinding(device_factory_provider_.get(),
                                        std::move(request));

  if (!factory_provider_client_connected_cb_.is_null()) {
    factory_provider_client_connected_cb_.Run();
  }
}

void ServiceImpl::OnTestingControlsRequest(
    mojom::TestingControlsRequest request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  mojo::MakeStrongBinding(
      std::make_unique<TestingControlsImpl>(ref_factory_->CreateRef()),
      std::move(request));
}

void ServiceImpl::LazyInitializeDeviceFactoryProvider() {
  if (device_factory_provider_)
    return;

  device_factory_provider_ = std::make_unique<DeviceFactoryProviderImpl>();
}

void ServiceImpl::OnProviderClientDisconnected() {
  // If last client has disconnected, release service ref so that service
  // shutdown timeout starts if no other references are still alive.
  // We keep the |device_factory_provider_| instance alive in order to avoid
  // losing state that would be expensive to reinitialize, e.g. having
  // already enumerated the available devices.
  if (factory_provider_bindings_.empty())
    device_factory_provider_->SetServiceRef(nullptr);

  if (!factory_provider_client_disconnected_cb_.is_null()) {
    factory_provider_client_disconnected_cb_.Run();
  }
}

}  // namespace video_capture
