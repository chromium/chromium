// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_binding.h"

#include <utility>

#include "base/bind.h"
#include "services/service_manager/public/cpp/service.h"

#include "base/debug/stack_trace.h"

namespace service_manager {

ServiceBinding::ServiceBinding(service_manager::Service* service)
    : service_(service), binding_(this) {
  DCHECK(service_);
}

ServiceBinding::ServiceBinding(service_manager::Service* service,
                               mojom::ServiceRequest request)
    : ServiceBinding(service) {
  if (request.is_pending())
    Bind(std::move(request));
}

ServiceBinding::~ServiceBinding() = default;

Connector* ServiceBinding::GetConnector() {
  if (!connector_)
    connector_ = Connector::Create(&pending_connector_request_);
  return connector_.get();
}

void ServiceBinding::Bind(mojom::ServiceRequest request) {
  DCHECK(!is_bound());
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(base::BindOnce(
      &ServiceBinding::OnConnectionError, base::Unretained(this)));
}

void ServiceBinding::RequestClose() {
  DCHECK(is_bound());
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
  binding_.Close();
  service_control_.reset();
  connector_.reset();
}

void ServiceBinding::OnConnectionError() {
  service_->OnDisconnected();
}

void ServiceBinding::OnStart(const Identity& identity,
                             OnStartCallback callback) {
  identity_ = identity;
  service_->OnStart();

  if (!pending_connector_request_.is_pending())
    connector_ = Connector::Create(&pending_connector_request_);
  std::move(callback).Run(std::move(pending_connector_request_),
                          mojo::MakeRequest(&service_control_));

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

  service_->OnBindInterface(source_info, interface_name,
                            std::move(interface_pipe));
}

}  // namespace service_manager
