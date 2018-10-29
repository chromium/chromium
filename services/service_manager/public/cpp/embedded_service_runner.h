// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_EMBEDDED_SERVICE_RUNNER_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_EMBEDDED_SERVICE_RUNNER_H_

#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "services/service_manager/public/cpp/embedded_service_info.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace service_manager {

class EmbeddedInstanceManager;

// Hosts in-process service instances for a given service.
class COMPONENT_EXPORT(SERVICE_MANAGER_CPP) EmbeddedServiceRunner {
 public:
  // Constructs a runner for a service. Every new instance started by the
  // Service Manager for this service will invoke the factory function on |info|
  // to create a new concrete instance of the Service implementation.
  EmbeddedServiceRunner(const base::StringPiece& name,
                        const EmbeddedServiceInfo& info);
  ~EmbeddedServiceRunner();

  // Binds an incoming ServiceRequest for this service. This creates a new
  // instance of the Service implementation.
  void BindServiceRequest(service_manager::mojom::ServiceRequest request);

  // Sets a callback to run when all instances of the service have stopped.
  void SetQuitClosure(const base::Closure& quit_closure);

 private:
  class InstanceManager;

  void OnQuit();

  // A reference to the instance manager, which may operate on another thread.
  scoped_refptr<EmbeddedInstanceManager> instance_manager_;

  base::Closure quit_closure_;

  base::WeakPtrFactory<EmbeddedServiceRunner> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedServiceRunner);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_EMBEDDED_SERVICE_RUNNER_H_
