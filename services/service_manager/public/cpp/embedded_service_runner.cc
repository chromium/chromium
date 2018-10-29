// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/embedded_service_runner.h"

#include "base/bind.h"
#include "services/service_manager/public/cpp/embedded_instance_manager.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace service_manager {

EmbeddedServiceRunner::EmbeddedServiceRunner(const base::StringPiece& name,
                                             const EmbeddedServiceInfo& info)
    : weak_factory_(this) {
  instance_manager_ = new EmbeddedInstanceManager(
      name, info,
      base::Bind(&EmbeddedServiceRunner::OnQuit, weak_factory_.GetWeakPtr()));
}

EmbeddedServiceRunner::~EmbeddedServiceRunner() {
  instance_manager_->ShutDown();
  instance_manager_ = nullptr;
}

void EmbeddedServiceRunner::BindServiceRequest(
    service_manager::mojom::ServiceRequest request) {
  instance_manager_->BindServiceRequest(std::move(request));
}

void EmbeddedServiceRunner::SetQuitClosure(const base::Closure& quit_closure) {
  quit_closure_ = quit_closure;
}

void EmbeddedServiceRunner::OnQuit() {
  if (!quit_closure_.is_null())
    quit_closure_.Run();
}

}  // namespace service_manager
