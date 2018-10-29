// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SERVICE_MANAGER_CONNECTION_H_
#define IOS_WEB_PUBLIC_SERVICE_MANAGER_CONNECTION_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/sequenced_task_runner.h"
#include "services/service_manager/public/cpp/embedded_service_info.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace service_manager {
class Connector;
}

namespace web {

// Encapsulates a connection to a //services/service_manager.
// Access a global instance on the thread the ServiceContext was bound by
// calling Holder::Get().
// Clients can add service_manager::Service implementations whose exposed
// interfaces
// will be exposed to inbound connections to this object's Service.
// Alternatively clients can define named services that will be constructed when
// requests for those service names are received.
// Clients must call any of the registration methods when receiving
// WebClient::RegisterInProcessServices().
class ServiceManagerConnection {
 public:
  using ServiceRequestHandler =
      base::Callback<void(service_manager::mojom::ServiceRequest)>;
  using Factory =
      base::Callback<std::unique_ptr<ServiceManagerConnection>(void)>;

  // Sets |connection| as the connection that is globally accessible from the
  // UI thread. Should be called on the UI thread.
  static void Set(std::unique_ptr<ServiceManagerConnection> connection);

  // Returns the global instance, or nullptr if the Service Manager
  // connection has not yet been bound. Should be called on the UI thread.
  static ServiceManagerConnection* Get();

  // Destroys the global instance. Should be called on the UI thread.
  static void Destroy();

  virtual ~ServiceManagerConnection();

  // Creates a ServiceManagerConnection from |request|. The connection binds
  // its interfaces and accept new connections on |io_task_runner| only. Note
  // that no incoming connections are accepted until Start() is called.
  static std::unique_ptr<ServiceManagerConnection> Create(
      service_manager::mojom::ServiceRequest request,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  // Begins accepting incoming connections.
  virtual void Start() = 0;

  // Returns the service_manager::Connector received via this connection's
  // Service implementation. Use this to initiate connections as this object's
  // Identity.
  virtual service_manager::Connector* GetConnector() = 0;

  // Adds an embedded service to this connection's ServiceFactory.
  // |info| provides details on how to construct new instances of the
  // service when an incoming connection is made to |name|.
  virtual void AddEmbeddedService(
      const std::string& name,
      const service_manager::EmbeddedServiceInfo& info) = 0;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_SERVICE_MANAGER_CONNECTION_H_
