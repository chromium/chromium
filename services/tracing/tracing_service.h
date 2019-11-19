// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_TRACING_SERVICE_H_
#define SERVICES_TRACING_TRACING_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace tracing {

class ServiceListener;

class TracingService : public service_manager::Service {
 public:
  explicit TracingService(service_manager::mojom::ServiceRequest request);
  ~TracingService() override;

  // service_manager::Service:
  void OnStart() override;
  void OnDisconnected() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

 private:
  service_manager::ServiceBinding service_binding_;

  service_manager::BinderRegistryWithArgs<
      const service_manager::BindSourceInfo&>
      registry_;

  std::unique_ptr<ServiceListener> service_listener_;

  // WeakPtrFactory members should always come last so WeakPtrs are destructed
  // before other members.
  base::WeakPtrFactory<TracingService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TracingService);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_TRACING_SERVICE_H_
