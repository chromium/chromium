// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_factory.h"

#include <utility>

#include "base/bind.h"
#include "content/public/common/content_client.h"
#include "services/service_manager/public/cpp/embedded_service_runner.h"

namespace content {

ServiceFactory::ServiceFactory() {}
ServiceFactory::~ServiceFactory() {}

void ServiceFactory::CreateService(
    service_manager::mojom::ServiceRequest request,
    const std::string& name,
    service_manager::mojom::PIDReceiverPtr pid_receiver) {
  // Only register services on first run.
  if (!has_registered_services_) {
    DCHECK(services_.empty());
    ServiceMap services;
    RegisterServices(&services);
    for (const auto& service : services) {
      std::unique_ptr<service_manager::EmbeddedServiceRunner> runner(
          new service_manager::EmbeddedServiceRunner(service.first,
                                                     service.second));
      runner->SetQuitClosure(base::Bind(&ServiceFactory::OnServiceQuit,
                                        base::Unretained(this)));
      services_.insert(std::make_pair(service.first, std::move(runner)));
    }
    has_registered_services_ = true;
  }

  auto it = services_.find(name);
  if (it == services_.end()) {
    // DCHECK in developer builds to make these errors easier to identify.
    // Otherwise they result only in cryptic browser error messages.
    NOTREACHED() << "Unable to start service \"" << name << "\". Did you "
                 << "forget to register the service in the utility process's"
                 << "ServiceFactory?";
    OnLoadFailed();
    return;
  }

  it->second->BindServiceRequest(std::move(request));
}

}  // namespace content
