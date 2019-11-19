// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_MANAGER_SERVICE_MANAGER_CONTEXT_H_
#define CONTENT_BROWSER_SERVICE_MANAGER_SERVICE_MANAGER_CONTEXT_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace service_manager {
class Connector;
}

namespace content {

// ServiceManagerContext manages the browser's connection to the ServiceManager,
// hosting a new in-process ServiceManagerContext if the browser was not
// launched from an external one.
class CONTENT_EXPORT ServiceManagerContext {
 public:
  explicit ServiceManagerContext(scoped_refptr<base::SingleThreadTaskRunner>
                                     service_manager_thread_task_runner);
  ~ServiceManagerContext();

  // Returns a service_manager::Connector that can be used on the IO thread.
  static service_manager::Connector* GetConnectorForIOThread();

  // Returns true if there is a valid process for |process_group_name|. Must be
  // called on the IO thread.
  static bool HasValidProcessForProcessGroup(
      const std::string& process_group_name);

  // Shutdowns the ServiceManager and the connections to the ServiceManager.
  void ShutDown();

 private:
  class InProcessServiceManagerContext;

  void RunServiceInstance(
      const service_manager::Identity& identity,
      mojo::PendingReceiver<service_manager::mojom::Service> receiver);

  scoped_refptr<base::SingleThreadTaskRunner>
      service_manager_thread_task_runner_;
  scoped_refptr<InProcessServiceManagerContext> in_process_context_;
  base::WeakPtrFactory<ServiceManagerContext> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceManagerContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_MANAGER_SERVICE_MANAGER_CONTEXT_H_
