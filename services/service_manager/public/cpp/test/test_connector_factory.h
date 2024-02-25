// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_TEST_TEST_CONNECTOR_FACTORY_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_TEST_TEST_CONNECTOR_FACTORY_H_

#include <map>
#include <memory>
#include <string>

#include "base/token.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/service_control.mojom.h"

namespace service_manager {

namespace mojom {
class Service;
}  // namespace mojom

// Creates Connector instances which route BindInterface requests directly to
// manually registered Service implementations. Useful for testing production
// code which is parameterized over a Connector, while bypassing all the
// Service Manager machinery. Typical usage should look something like:
//
//     TEST(MyTest, Foo) {
//       base::test::TaskEnvironment task_environment;
//       TestConnectorFactory connector_factory;
//       my_service::MyServiceImpl service(connector_factory.RegisterInstance(
//           my_service::mojom::kServiceName));
//
//       RunSomeClientCode(connector_factory.GetDefaultConnector());
//     }
//
// Where |RunSomeClientCode()| would typically be some production code that
// expects a functioning Connector and uses it to connect to the service you're
// testing.
class TestConnectorFactory : public mojom::ServiceControl {
 public:
  // Creates a simple TestConnectorFactory which can be used register Service
  // instances and vend Connectors which can connect to them.
  TestConnectorFactory();

  TestConnectorFactory(const TestConnectorFactory&) = delete;
  TestConnectorFactory& operator=(const TestConnectorFactory&) = delete;

  ~TestConnectorFactory() override;

  // A mapping from service names to Service proxies for registered instances.
  using NameToServiceProxyMap =
      std::map<std::string, mojo::Remote<mojom::Service>>;

  // A Connector which can be used to connect to any service instances
  // registered with this object. This Connector identifies its source as a
  // generic meaningless Identity.
  Connector* GetDefaultConnector();

  // Creates a new connector which routes BindInterfaces requests directly to
  // the Service instance associated with this factory.
  std::unique_ptr<Connector> CreateConnector();

  // Registers a Service instance not owned by this TestConnectorFactory.
  // Returns a ServiceRequest which the instance must bind in order to receive
  // simulated events from this object.
  mojo::PendingReceiver<mojom::Service> RegisterInstance(
      const std::string& service_name);

  const base::Token& test_instance_group() const {
    return test_instance_group_;
  }

  // Normally a TestConnectorFactory will assert if asked to route a request to
  // an unregistered service. If this is set to |true|, such requests will be
  // silently ignored instead.
  bool ignore_unknown_service_requests() const {
    return ignore_unknown_service_requests_;
  }
  void set_ignore_unknown_service_requests(bool ignore) {
    ignore_unknown_service_requests_ = ignore;
  }

  // Normally when a service instance registered via |RegisterInstance()|
  // requests termination from the Service Manager, TestConnectorFactory
  // immediately severs the service instance's connection, typically
  // triggering the service's shutdown path.
  //
  // If this is set to |true| (defaults to |false|), quit requests are ignored
  // and each service instance will remain connected to the TestConnectorFactory
  // until either it or the TestConnectorFactory is destroyed.
  void set_ignore_quit_requests(bool ignore) { ignore_quit_requests_ = ignore; }

 private:
  void OnStartResponseHandler(
      const std::string& service_name,
      mojo::PendingReceiver<mojom::Connector> connector_receiver,
      mojo::PendingAssociatedReceiver<mojom::ServiceControl> control_receiver);

  // mojom::ServiceControl:
  void RequestQuit() override;

  std::unique_ptr<mojom::Connector> impl_;
  base::Token test_instance_group_;
  std::unique_ptr<Connector> default_connector_;

  // Mapping used only in the default-constructed case where Service instances
  // are unowned by the TestConnectorFactory. Maps service names to their
  // proxies.
  NameToServiceProxyMap service_proxies_;

  // ServiceControl bindings which receive and process RequestQuit requests from
  // connected service instances. The associated service name is used as
  // context.
  mojo::AssociatedReceiverSet<mojom::ServiceControl, std::string>
      service_control_receivers_;

  bool ignore_unknown_service_requests_ = false;
  bool ignore_quit_requests_ = false;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_TEST_TEST_CONNECTOR_FACTORY_H_
