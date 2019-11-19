// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_BINDING_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_BINDING_H_

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/public/mojom/service_control.mojom.h"

namespace service_manager {

class Service;

// Encapsulates service-side bindings to Service Manager interfaces. Namely,
// this helps receive and dispatch Service interface events to a service
// implementation, while also exposing a working Connector interface the service
// can use to make outgoing interface requests.
//
// A ServiceBinding is considered to be "bound" after |Bind()| is invoked with a
// valid Service receiver (or the equivalent constructor is used -- see below).
// Upon connection error or an explicit call to |Close()|, the ServiceBinding
// will be considered "unbound" until another call to |Bind()| is made.
//
// NOTE: A well-behaved service should aim to always close its ServiceBinding
// gracefully by calling |RequestClose()|. Closing a ServiceBinding abruptly
// (by either destroying it or explicitly calling |Close()|) introduces inherent
// flakiness into the system unless the Service's |OnDisconnected()| has already
// been invoked, because otherwise the Service Manager may have in-flight
// interface requests directed at your service instance and these will be
// dropped to the dismay of the service instance which issued them. Exceptions
// can reasonably be made for system-wide shutdown situations where even the
// Service Manager itself will be imminently torn down.
class COMPONENT_EXPORT(SERVICE_MANAGER_CPP) ServiceBinding
    : public mojom::Service {
 public:
  // Creates a new ServiceBinding bound to |service|. The service will not
  // receive any Service interface calls until |Bind()| is called, but its
  // |connector()| is usable immediately upon construction.
  //
  // |service| is not owned and must outlive this ServiceBinding.
  explicit ServiceBinding(service_manager::Service* service);

  // Same as above, but behaves as if |Bind(receiver)| is also called
  // immediately after construction. See below.
  ServiceBinding(service_manager::Service* service,
                 mojo::PendingReceiver<mojom::Service> receiver);

  ~ServiceBinding() override;

  bool is_bound() const { return receiver_.is_bound(); }

  const Identity& identity() const { return identity_; }

  // Returns a usable Connector which can make outgoing interface requests
  // identifying as the service to which this ServiceBinding is bound.
  Connector* GetConnector();

  // Binds this ServiceBinding to a new Service receiver. Once a ServiceBinding
  // is bound, its target Service will begin receiving Service events. The
  // order of events received is:
  //
  //   - OnStart() exactly once
  //   - OnIdentityKnown() exactly once
  //   - OnBindInterface() zero or more times
  //
  // The target Service will be able to receive these events until this
  // ServiceBinding is either unbound or destroyed.
  //
  // If |receiver| is invalid, this call does nothing.
  //
  // Must only be called on an unbound ServiceBinding.
  void Bind(mojo::PendingReceiver<mojom::Service> receiver);

  // Asks the Service Manager nicely if it's OK for this service instance to
  // disappear now. If the Service Manager thinks it's OK, it will sever the
  // binding's connection, ultimately triggering an |OnDisconnected()| call on
  // the bound Service object.
  //
  // Must only be called on a bound ServiceBinding.
  void RequestClose();

  // Immediately severs the connection to the Service Manager. No further
  // incoming interface requests will be received until this ServiceBinding is
  // bound again. Always prefer |RequestClose()| under normal circumstances,
  // unless |OnDisconnected()| has already been invoked on the Service. See the
  // note in the class documentation above regarding graceful binding closure.
  //
  // Must only be called on a bound ServiceBinding.
  void Close();

  // Allows the caller to intercept all requests for a specific interface
  // targeting any instance of the service |service_name| running in the calling
  // process. Prefer the template helpers for clarity.
  using BinderForTesting =
      base::RepeatingCallback<void(const BindSourceInfo&,
                                   mojo::ScopedMessagePipeHandle)>;
  static void OverrideInterfaceBinderForTesting(
      const std::string& service_name,
      const std::string& interface_name,
      const BinderForTesting& binder);
  static void ClearInterfaceBinderOverrideForTesting(
      const std::string& service_name,
      const std::string& interface_name);

  template <typename Interface>
  using TypedBinderWithInfoForTesting =
      base::RepeatingCallback<void(const BindSourceInfo&,
                                   mojo::PendingReceiver<Interface>)>;
  template <typename Interface>
  static void OverrideInterfaceBinderForTesting(
      const std::string& service_name,
      const TypedBinderWithInfoForTesting<Interface>& binder) {
    ServiceBinding::OverrideInterfaceBinderForTesting(
        service_name, Interface::Name_,
        base::BindRepeating(
            [](const TypedBinderWithInfoForTesting<Interface>& binder,
               const BindSourceInfo& info, mojo::ScopedMessagePipeHandle pipe) {
              binder.Run(info,
                         mojo::PendingReceiver<Interface>(std::move(pipe)));
            },
            binder));
  }
  template <typename Interface>
  using TypedBinderForTesting =
      base::RepeatingCallback<void(mojo::PendingReceiver<Interface>)>;
  template <typename Interface>
  static void OverrideInterfaceBinderForTesting(
      const std::string& service_name,
      const TypedBinderForTesting<Interface>& binder) {
    ServiceBinding::OverrideInterfaceBinderForTesting(
        service_name, base::BindRepeating(
                          [](const TypedBinderForTesting<Interface>& binder,
                             const BindSourceInfo& info,
                             mojo::PendingReceiver<Interface> receiver) {
                            binder.Run(std::move(receiver));
                          },
                          binder));
  }

  template <typename Interface>
  static void ClearInterfaceBinderOverrideForTesting(
      const std::string& service_name) {
    ClearInterfaceBinderOverrideForTesting(service_name, Interface::Name_);
  }

 private:
  void OnConnectionError();

  // mojom::Service:
  void OnStart(const Identity& identity, OnStartCallback callback) override;
  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe,
                       OnBindInterfaceCallback callback) override;
  void CreatePackagedServiceInstance(
      const Identity& identity,
      mojo::PendingReceiver<mojom::Service> receiver,
      mojo::PendingRemote<mojom::ProcessMetadata> metadata) override;

  // The Service instance to which all incoming events from the Service Manager
  // should be directed. Typically this is the object which owns this
  // ServiceBinding.
  service_manager::Service* const service_;

  // A pending Connector request which will eventually be passed to the Service
  // Manager. Created preemptively by every unbound ServiceBinding so that
  // |connector()| may begin pipelining outgoing requests even before the
  // ServiceBinding is bound to a Service receiver.
  mojo::PendingReceiver<mojom::Connector> pending_connector_receiver_;

  mojo::Receiver<mojom::Service> receiver_{this};
  Identity identity_;
  std::unique_ptr<Connector> connector_;

  // This instance's control interface to the service manager. Note that this
  // is unbound and therefore invalid until OnStart() is called.
  mojo::AssociatedRemote<mojom::ServiceControl> service_control_;

  // Tracks whether |RequestClose()| has been called at least once prior to
  // receiving |OnStart()| on a bound ServiceBinding. This ensures that the
  // closure request is actually issued once |OnStart()| is invoked.
  bool request_closure_on_start_ = false;

  DISALLOW_COPY_AND_ASSIGN(ServiceBinding);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_CONTEXT_H_
