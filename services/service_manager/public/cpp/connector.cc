// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/connector.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace service_manager {

Connector::Connector(mojo::PendingRemote<mojom::Connector> unbound_state)
    : unbound_state_(std::move(unbound_state)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

Connector::Connector(mojo::Remote<mojom::Connector> connector)
    : connector_(std::move(connector)) {
  connector_.set_disconnect_handler(
      base::BindOnce(&Connector::OnConnectionError, base::Unretained(this)));
}

Connector::~Connector() = default;

std::unique_ptr<Connector> Connector::Create(
    mojo::PendingReceiver<mojom::Connector>* receiver) {
  mojo::PendingRemote<mojom::Connector> proxy;
  *receiver = proxy.InitWithNewPipeAndPassReceiver();
  return std::make_unique<Connector>(std::move(proxy));
}

void Connector::WarmService(const ServiceFilter& filter,
                            WarmServiceCallback callback) {
  if (!BindConnectorIfNecessary())
    return;
  connector_->WarmService(filter, std::move(callback));
}

void Connector::RegisterServiceInstance(
    const Identity& identity,
    mojo::PendingRemote<mojom::Service> service,
    mojo::PendingReceiver<mojom::ProcessMetadata> metadata_receiver,
    RegisterServiceInstanceCallback callback) {
  if (!BindConnectorIfNecessary())
    return;

  DCHECK(identity.IsValid());
  DCHECK(service);
  connector_->RegisterServiceInstance(identity, service.PassPipe(),
                                      std::move(metadata_receiver),
                                      std::move(callback));
}

void Connector::QueryService(const std::string& service_name,
                             mojom::Connector::QueryServiceCallback callback) {
  if (!BindConnectorIfNecessary())
    return;

  connector_->QueryService(service_name, std::move(callback));
}

void Connector::BindInterface(const ServiceFilter& filter,
                              const std::string& interface_name,
                              mojo::ScopedMessagePipeHandle interface_pipe,
                              mojom::BindInterfacePriority priority,
                              BindInterfaceCallback callback) {
  auto service_overrides_iter = local_binder_overrides_.find(filter);
  if (service_overrides_iter != local_binder_overrides_.end()) {
    auto override_iter = service_overrides_iter->second.find(interface_name);
    if (override_iter != service_overrides_iter->second.end()) {
      override_iter->second.Run(std::move(interface_pipe));
      return;
    }
  }

  if (!BindConnectorIfNecessary())
    return;

  connector_->BindInterface(filter, interface_name, std::move(interface_pipe),
                            priority, std::move(callback));
}

std::unique_ptr<Connector> Connector::Clone() {
  mojo::PendingRemote<mojom::Connector> connector;
  auto receiver = connector.InitWithNewPipeAndPassReceiver();
  if (BindConnectorIfNecessary())
    connector_->Clone(std::move(receiver));
  return std::make_unique<Connector>(std::move(connector));
}

bool Connector::IsBound() const {
  return connector_.is_bound();
}

void Connector::BindConnectorReceiver(
    mojo::PendingReceiver<mojom::Connector> receiver) {
  if (!BindConnectorIfNecessary())
    return;
  connector_->Clone(std::move(receiver));
}

base::WeakPtr<Connector> Connector::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void Connector::OverrideBinderForTesting(
    const service_manager::ServiceFilter& filter,
    const std::string& interface_name,
    const TestApi::Binder& binder) {
  local_binder_overrides_[filter][interface_name] = binder;
}

bool Connector::HasBinderOverrideForTesting(
    const service_manager::ServiceFilter& filter,
    const std::string& interface_name) {
  auto service_overrides = local_binder_overrides_.find(filter);
  if (service_overrides == local_binder_overrides_.end())
    return false;

  return base::Contains(service_overrides->second, interface_name);
}

void Connector::ClearBinderOverrideForTesting(
    const service_manager::ServiceFilter& filter,
    const std::string& interface_name) {
  auto service_overrides = local_binder_overrides_.find(filter);
  if (service_overrides == local_binder_overrides_.end())
    return;

  service_overrides->second.erase(interface_name);
}

void Connector::ClearBinderOverridesForTesting() {
  local_binder_overrides_.clear();
}

void Connector::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connector_.reset();
}

bool Connector::BindConnectorIfNecessary() {
  // Bind the message pipe and SequenceChecker to the current thread the first
  // time it is used to connect.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connector_.is_bound()) {
    if (!unbound_state_.is_valid()) {
      // It's possible to get here when the link to the service manager has been
      // severed (and so the connector pipe has been closed) but the app has
      // chosen not to quit.
      return false;
    }

    connector_.Bind(std::move(unbound_state_));
    connector_.set_disconnect_handler(
        base::BindOnce(&Connector::OnConnectionError, base::Unretained(this)));
  }

  return true;
}

}  // namespace service_manager
