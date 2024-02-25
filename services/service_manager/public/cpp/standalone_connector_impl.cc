// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/standalone_connector_impl.h"

#include "base/check.h"
#include "base/notreached.h"

namespace service_manager {

StandaloneConnectorImpl::StandaloneConnectorImpl(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

StandaloneConnectorImpl::~StandaloneConnectorImpl() = default;

mojo::PendingRemote<mojom::Connector> StandaloneConnectorImpl::MakeRemote() {
  mojo::PendingRemote<mojom::Connector> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void StandaloneConnectorImpl::BindInterface(
    const ServiceFilter& filter,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe,
    mojom::BindInterfacePriority priority,
    BindInterfaceCallback callback) {
  delegate_->OnConnect(
      filter.service_name(),
      mojo::GenericPendingReceiver(interface_name, std::move(interface_pipe)));
  std::move(callback).Run(mojom::ConnectResult::SUCCEEDED, std::nullopt);
}

void StandaloneConnectorImpl::QueryService(const std::string& service_name,
                                           QueryServiceCallback callback) {
  NOTIMPLEMENTED()
      << "QueryService is not supported by StandaloneConnectorImpl.";
  std::move(callback).Run(nullptr);
}

void StandaloneConnectorImpl::WarmService(const ServiceFilter& filter,
                                          WarmServiceCallback callback) {
  NOTIMPLEMENTED()
      << "WarmService is not supported by StandaloneConnectorImpl.";
  std::move(callback).Run(mojom::ConnectResult::INVALID_ARGUMENT, std::nullopt);
}

void StandaloneConnectorImpl::RegisterServiceInstance(
    const Identity& identity,
    mojo::ScopedMessagePipeHandle service_pipe,
    mojo::PendingReceiver<mojom::ProcessMetadata> metadata_receiver,
    RegisterServiceInstanceCallback callback) {
  NOTIMPLEMENTED()
      << "RegisterServiceInstance is not supported by StandaloneConnectorImpl.";
  std::move(callback).Run(mojom::ConnectResult::INVALID_ARGUMENT);
}

void StandaloneConnectorImpl::Clone(
    mojo::PendingReceiver<mojom::Connector> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace service_manager
