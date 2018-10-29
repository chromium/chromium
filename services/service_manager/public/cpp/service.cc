// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service.h"

#include "base/logging.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace service_manager {

Service::Service() = default;

Service::~Service() = default;

void Service::OnStart() {}

void Service::OnBindInterface(const BindSourceInfo& source,
                              const std::string& interface_name,
                              mojo::ScopedMessagePipeHandle interface_pipe) {}

void Service::OnDisconnected() {}

bool Service::OnServiceManagerConnectionLost() {
  return true;
}

ServiceContext* Service::context() const {
  DCHECK(service_context_)
      << "Service::context() may only be called after the Service constructor.";
  return service_context_;
}

void Service::SetContext(ServiceContext* context) {
  service_context_ = context;
}

ForwardingService::ForwardingService(Service* target) : target_(target) {}

ForwardingService::~ForwardingService() {}

void ForwardingService::OnStart() {
  target_->OnStart();
}

void ForwardingService::OnBindInterface(
    const BindSourceInfo& source,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  target_->OnBindInterface(source, interface_name, std::move(interface_pipe));
}

bool ForwardingService::OnServiceManagerConnectionLost() {
  return target_->OnServiceManagerConnectionLost();
}

void ForwardingService::SetContext(ServiceContext* context) {
  target_->SetContext(context);
}

}  // namespace service_manager
