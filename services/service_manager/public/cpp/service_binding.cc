// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_binding.h"

#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/tracing/public/cpp/traced_process.h"
#include "services/tracing/public/mojom/traced_process.mojom.h"

namespace service_manager {

namespace {

// Thread-safe mapping of all registered binder overrides in the process.
class BinderOverrides {
 public:
  BinderOverrides() = default;
  ~BinderOverrides() = default;

  void SetOverride(const std::string& service_name,
                   const std::string& interface_name,
                   const ServiceBinding::BinderForTesting& binder) {
    base::AutoLock lock(lock_);
    binders_[service_name][interface_name] = binder;
  }

  ServiceBinding::BinderForTesting GetOverride(
      const std::string& service_name,
      const std::string& interface_name) {
    base::AutoLock lock(lock_);
    auto service_it = binders_.find(service_name);
    if (service_it == binders_.end())
      return ServiceBinding::BinderForTesting();
    auto binder_it = service_it->second.find(interface_name);
    if (binder_it == service_it->second.end())
      return ServiceBinding::BinderForTesting();
    return binder_it->second;
  }

  void ClearOverride(const std::string& service_name,
                     const std::string& interface_name) {
    base::AutoLock lock(lock_);
    auto service_it = binders_.find(service_name);
    if (service_it == binders_.end())
      return;
    service_it->second.erase(interface_name);
    if (service_it->second.empty())
      binders_.erase(service_it);
  }

 private:
  base::Lock lock_;

  using InterfaceBinderMap =
      std::map<std::string, ServiceBinding::BinderForTesting>;
  std::map<std::string, InterfaceBinderMap> binders_;

  DISALLOW_COPY_AND_ASSIGN(BinderOverrides);
};

BinderOverrides& GetBinderOverrides() {
  static base::NoDestructor<BinderOverrides> overrides;
  return *overrides;
}

}  // namespace

ServiceBinding::ServiceBinding(service_manager::Service* service)
    : service_(service) {
  DCHECK(service_);
}

ServiceBinding::ServiceBinding(service_manager::Service* service,
                               mojo::PendingReceiver<mojom::Service> receiver)
    : ServiceBinding(service) {
  if (receiver.is_valid())
    Bind(std::move(receiver));
}

ServiceBinding::~ServiceBinding() = default;

Connector* ServiceBinding::GetConnector() {
  if (!connector_)
    connector_ = Connector::Create(&pending_connector_receiver_);
  return connector_.get();
}

void ServiceBinding::Bind(mojo::PendingReceiver<mojom::Service> receiver) {
  DCHECK(!is_bound());
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &ServiceBinding::OnConnectionError, base::Unretained(this)));
}

void ServiceBinding::RequestClose() {
  // We allow for innoccuous RequestClose() calls on unbound ServiceBindings.
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

void ServiceBinding::Close() {
  DCHECK(is_bound());
  receiver_.reset();
  service_control_.reset();
  connector_.reset();
}

// static
void ServiceBinding::OverrideInterfaceBinderForTesting(
    const std::string& service_name,
    const std::string& interface_name,
    const BinderForTesting& binder) {
  GetBinderOverrides().SetOverride(service_name, interface_name, binder);
}

// static
void ServiceBinding::ClearInterfaceBinderOverrideForTesting(
    const std::string& service_name,
    const std::string& interface_name) {
  GetBinderOverrides().ClearOverride(service_name, interface_name);
}

void ServiceBinding::OnConnectionError() {
  service_->OnDisconnected();
}

void ServiceBinding::OnStart(const Identity& identity,
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

void ServiceBinding::OnBindInterface(
    const BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe,
    OnBindInterfaceCallback callback) {
  // Acknowledge this request.
  std::move(callback).Run();

  auto override =
      GetBinderOverrides().GetOverride(identity_.name(), interface_name);
  if (override) {
    override.Run(source_info, std::move(interface_pipe));
    return;
  }

  if (interface_name == tracing::mojom::TracedProcess::Name_) {
    tracing::TracedProcess::OnTracedProcessRequest(
        tracing::mojom::TracedProcessRequest(std::move(interface_pipe)));
    return;
  }

  service_->OnConnect(source_info, interface_name, std::move(interface_pipe));
}

void ServiceBinding::CreatePackagedServiceInstance(
    const Identity& identity,
    mojo::PendingReceiver<mojom::Service> receiver,
    mojo::PendingRemote<mojom::ProcessMetadata> metadata) {
  service_->CreatePackagedServiceInstance(
      identity.name(), std::move(receiver),
      base::BindOnce(
          [](mojo::PendingRemote<mojom::ProcessMetadata> pending_metadata,
             base::Optional<base::ProcessId> pid) {
            if (pid) {
              mojo::Remote<mojom::ProcessMetadata> metadata(
                  std::move(pending_metadata));
              metadata->SetPID(*pid);
            }
          },
          std::move(metadata)));
}

}  // namespace service_manager
