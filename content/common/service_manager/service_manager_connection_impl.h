// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_MANAGER_SERVICE_MANAGER_CONNECTION_IMPL_H_
#define CONTENT_COMMON_SERVICE_MANAGER_SERVICE_MANAGER_CONNECTION_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace service_manager {
class Connector;
}

namespace content {

class CONTENT_EXPORT ServiceManagerConnectionImpl
    : public ServiceManagerConnection {
 public:
  explicit ServiceManagerConnectionImpl(
      service_manager::mojom::ServiceRequest request,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  ~ServiceManagerConnectionImpl() override;

 private:
  class IOThreadContext;

  // ServiceManagerConnection:
  void Start() override;
  void Stop() override;
  service_manager::Connector* GetConnector() override;
  void SetConnectionLostClosure(const base::Closure& closure) override;
  int AddConnectionFilter(std::unique_ptr<ConnectionFilter> filter) override;
  void RemoveConnectionFilter(int filter_id) override;
  void AddServiceRequestHandler(
      const std::string& name,
      const ServiceRequestHandler& handler) override;
  void AddServiceRequestHandlerWithCallback(
      const std::string& name,
      const ServiceRequestHandlerWithCallback& handler) override;
  void SetDefaultServiceRequestHandler(
      const DefaultServiceRequestHandler& handler) override;

  void OnConnectionLost();
  void GetInterface(service_manager::mojom::InterfaceProvider* provider,
                    const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle request_handle);

  std::unique_ptr<service_manager::Connector> connector_;
  scoped_refptr<IOThreadContext> context_;

  base::Closure connection_lost_handler_;

  base::WeakPtrFactory<ServiceManagerConnectionImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceManagerConnectionImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_MANAGER_SERVICE_MANAGER_CONNECTION_IMPL_H_
