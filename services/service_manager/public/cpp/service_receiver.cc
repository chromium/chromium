// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_receiver.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/synchronization/lock.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/service.h"

namespace service_manager {

ServiceReceiver::ServiceReceiver(service_manager::Service* service)
    : service_(service) {
  DCHECK(service_);
}

ServiceReceiver::ServiceReceiver(service_manager::Service* service,
                                 mojo::PendingReceiver<mojom::Service> receiver)
    : ServiceReceiver(service) {
  if (receiver.is_valid())
    Bind(std::move(receiver));
}

ServiceReceiver::~ServiceReceiver() = default;

Connector* ServiceReceiver::GetConnector() {
  if (!connector_)
    connector_ = Connector::Create(&pending_connector_receiver_);
  return connector_.get();
}

void ServiceReceiver::Bind(mojo::PendingReceiver<mojom::Service> receiver) {
  DCHECK(!is_bound());
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &ServiceReceiver::OnConnectionError, base::Unretained(this)));
}

void ServiceReceiver::RequestClose() {
  // We allow for innoccuous RequestClose() calls on unbound ServiceReceivers.
  // This may occur e.g. when running a service in-process.
  if (!is_bound())
    return;

  if (service_control_.is_bound()) {
    service_control_->RequestQuit();
  } else {
    // It's possible that the service may request closure before receiving the
    // initial |OnStart()| event, in which case there is not yet a control
    // interface on which to request closure. In that case we defer until
    // |OnStart()| is received.
    request_closure_on_start_ = true;
  }
}

void ServiceReceiver::Close() {
  DCHECK(is_bound());
  receiver_.reset();
  service_control_.reset();
  connector_.reset();
}

void ServiceReceiver::OnConnectionError() {
  service_->OnDisconnected();
}

void ServiceReceiver::OnStart(const Identity& identity,
                              OnStartCallback callback) {
  identity_ = identity;

  if (!pending_connector_receiver_.is_valid())
    connector_ = Connector::Create(&pending_connector_receiver_);
  std::move(callback).Run(std::move(pending_connector_receiver_),
                          service_control_.BindNewEndpointAndPassReceiver());

  service_->OnStart();

  // Execute any prior |RequestClose()| request on the service's behalf.
  if (request_closure_on_start_)
    service_control_->RequestQuit();
}

void ServiceReceiver::OnBindInterface(
    const BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe,
    OnBindInterfaceCallback callback) {
  // Acknowledge this request.
  std::move(callback).Run();

  service_->OnConnect(source_info, interface_name, std::move(interface_pipe));
}

void ServiceReceiver::CreatePackagedServiceInstance(
    const Identity& identity,
    mojo::PendingReceiver<mojom::Service> receiver,
    mojo::PendingRemote<mojom::ProcessMetadata> metadata) {
  service_->CreatePackagedServiceInstance(
      identity.name(), std::move(receiver),
      base::BindOnce(
          [](mojo::PendingRemote<mojom::ProcessMetadata> pending_metadata,
             std::optional<base::ProcessId> pid) {
            if (pid) {
              mojo::Remote<mojom::ProcessMetadata> metadata(
                  std::move(pending_metadata));
              metadata->SetPID(*pid);
            }
          },
          std::move(metadata)));
}

}  // namespace service_manager
