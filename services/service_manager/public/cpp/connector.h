// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CONNECTOR_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CONNECTOR_H_

#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/export.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/service.mojom-forward.h"
#include "services/service_manager/public/mojom/service_manager.mojom-forward.h"

namespace service_manager {

// An interface that encapsulates the Service Manager's brokering interface, by
// which connections between services are established. Once any public methods
// are called on an instance of this class, that instance is bound to the
// calling thread.
//
// To use the same interface on another thread, call Clone() and pass the new
// instance to the desired thread before calling any public methods on it.
class SERVICE_MANAGER_PUBLIC_CPP_EXPORT Connector {
 public:
  // DEPRECATED: Do not introduce new uses of this API. Just call the public
  // *ForTesting methods directly on the Connector. Also see the note on those
  // methods about preferring TestConnectorFactory where feasible.
  class TestApi {
   public:
    using Binder = base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)>;
    explicit TestApi(Connector* connector) : connector_(connector) {}
    ~TestApi() = default;

    // Allows caller to specify a callback to bind requests for |interface_name|
    // from |service_name| locally, rather than passing the request through the
    // Service Manager.
    void OverrideBinderForTesting(const service_manager::ServiceFilter& filter,
                                  const std::string& interface_name,
                                  const Binder& binder) {
      connector_->OverrideBinderForTesting(filter, interface_name, binder);
    }
    bool HasBinderOverride(const service_manager::ServiceFilter& filter,
                           const std::string& interface_name) {
      return connector_->HasBinderOverrideForTesting(filter, interface_name);
    }
    void ClearBinderOverride(const service_manager::ServiceFilter& filter,
                             const std::string& interface_name) {
      connector_->ClearBinderOverrideForTesting(filter, interface_name);
    }
    void ClearBinderOverrides() {
      connector_->ClearBinderOverridesForTesting();
    }

   private:
    raw_ptr<Connector> connector_;
  };

  explicit Connector(mojo::PendingRemote<mojom::Connector> unbound_state);
  explicit Connector(mojo::Remote<mojom::Connector> connector);

  Connector(const Connector&) = delete;
  Connector& operator=(const Connector&) = delete;

  ~Connector();

  // Creates a new Connector instance and fills in |*receiver| with a request
  // for the other end the Connector's interface.
  static std::unique_ptr<Connector> Create(
      mojo::PendingReceiver<mojom::Connector>* receiver);

  // Asks the Service Manager to ensure that there's a running service instance
  // which would match |filter| from the caller's perspective. Useful when
  // future |BindInterface()| calls will be made with the same |filter| and
  // reducing their latency is a high priority.
  //
  // Note that there is no value in calling this *immediately* before
  // |BindInterface()|, so only use it if there is some delay between when you
  // know you will want to talk to a service and when you actually need to for
  // the first time. For example, Chrome warms the Network Service ASAP on
  // startup so that it can be brought up in parallel while the browser is
  // initializing other subsystems.
  //
  // |callback| conveys information about the result of the request. See
  // documentation on the mojom ConnectResult definition for the meaning of
  // |result|. If |result| is |SUCCEEDED|, then |identity| will contain the full
  // identity of the matching service instance, which was either already running
  // or was started as a result of this request.
  using WarmServiceCallback =
      base::OnceCallback<void(mojom::ConnectResult result,
                              const std::optional<Identity>& identity)>;
  void WarmService(const ServiceFilter& filter,
                   WarmServiceCallback callback = {});

  // Creates an instance of a service for |identity| in a process started by the
  // client (or someone else). Must be called before any |BindInterface()|
  // reaches the Service Manager expecting the instance to exist, otherwise the
  // Service Manager may attempt to start a new instance on its own, which will
  // invariably fail (if the Service Manager knew how to start the instance, the
  // caller wouldn't need to use this API).
  //
  // NOTE: |identity| must be a complete service Identity (including a random
  // globally unique ID), and this call will only succeed if the calling service
  // has set |can_create_other_service_instances| option to |true| in its
  // manifest. This is considered privileged behavior.
  using RegisterServiceInstanceCallback =
      base::OnceCallback<void(mojom::ConnectResult result)>;
  void RegisterServiceInstance(
      const Identity& identity,
      mojo::PendingRemote<mojom::Service> service,
      mojo::PendingReceiver<mojom::ProcessMetadata> metadata_receiver,
      RegisterServiceInstanceCallback callback = {});

  // Determines if the service for |service_name| is known, and returns
  // information about it from the catalog.
  void QueryService(const std::string& service_name,
                    mojom::Connector::QueryServiceCallback callback);

  // All variants of |Connect()| ask the Service Manager to route a
  // |mojo::Receiver<T>| for any interface type T to a service instance
  // identified by a ServiceFilter. If no running service instance matches the
  // provided ServiceFilter, the Service Manager may start a new instance which
  // does, before routing the Receiver to it.
  template <typename Interface>
  void Connect(const ServiceFilter& filter,
               mojo::PendingReceiver<Interface> receiver,
               mojom::BindInterfacePriority priority =
                   mojom::BindInterfacePriority::kImportant) {
    BindInterface(filter, std::move(receiver), priority);
  }

  // A variant of the above which constructs a simple ServiceFilter by service
  // name only. This will route the Receiver to any available instance of the
  // named service.
  template <typename Interface>
  void Connect(const std::string& service_name,
               mojo::PendingReceiver<Interface> receiver) {
    Connect(ServiceFilter::ByName(service_name), std::move(receiver));
  }

  // A variant of the above which take a callback, |callback| conveys
  // information about the result of the request. See documentation on the mojom
  // ConnectResult definition for the meaning of |result|. If |result| is
  // |SUCCEEDED|, then |identity| will contain the full identity of the matching
  // service instance to which the pending receiver was routed. This service
  // instance was either already running, or was started as a result of this
  // request.
  using BindInterfaceCallback =
      base::OnceCallback<void(mojom::ConnectResult result,
                              const std::optional<Identity>& identity)>;
  template <typename Interface>
  void Connect(const ServiceFilter& filter,
               mojo::PendingReceiver<Interface> receiver,
               BindInterfaceCallback callback) {
    BindInterface(filter, std::move(receiver), std::move(callback));
  }
  // DEPRECATED: Prefer |Connect()| above. |BindInterface()| uses deprecated
  // InterfaceRequest and InterfacePtr types.
  //
  // All variants of |BindInterface()| ask the Service Manager to route an
  // interface request to a service instance matching |filter|. If no running
  // service instance matches |filter|, the Service Manager may start a new
  // service instance which does, and then route the request to it. See the
  // comments in connector.mojom regarding restrictions on when the Service
  // Manager will *not* create a new instance or when |filter| may otherwise be
  // rejected.
  //
  // For variants which take a callback, |callback| conveys information about
  // the result of the request. See documentation on the mojom ConnectResult
  // definition for the meaning of |result|. If |result| is |SUCCEEDED|, then
  // |identity| will contain the full identity of the matching service instance
  // to which the interface request was routed. This service instance was either
  // already running, or was started as a result of this request.
  //
  // TODO: We should consider removing some of these overloads as they're quite
  // redundant. It would be cleaner to just have callers explicitly construct a
  // ServiceFilter and InterfaceRequest rather than having a bunch of template
  // helpers to do the same in various combinations. The first and last variants
  // of |BindInterface()| here are sufficient for all use cases.
  template <typename Interface>
  void BindInterface(const ServiceFilter& filter,
                     mojo::PendingReceiver<Interface> receiver,
                     BindInterfaceCallback callback = {}) {
    BindInterface(filter, Interface::Name_, receiver.PassPipe(),
                  mojom::BindInterfacePriority::kImportant,
                  std::move(callback));
  }

  template <typename Interface>
  void BindInterface(const std::string& service_name,
                     mojo::PendingReceiver<Interface> receiver) {
    return BindInterface(ServiceFilter::ByName(service_name),
                         std::move(receiver));
  }

  template <typename Interface>
  void BindInterface(const ServiceFilter& filter,
                     mojo::PendingReceiver<Interface> receiver,
                     mojom::BindInterfacePriority priority) {
    return BindInterface(filter, Interface::Name_, receiver.PassPipe(),
                         priority, {});
  }

  void BindInterface(const ServiceFilter& filter,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe,
                     BindInterfaceCallback callback = {}) {
    BindInterface(filter, interface_name, std::move(interface_pipe),
                  mojom::BindInterfacePriority::kImportant,
                  std::move(callback));
  }

  void BindInterface(const ServiceFilter& filter,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe,
                     mojom::BindInterfacePriority priority,
                     BindInterfaceCallback callback);

  // Creates a new instance of this class which may be passed to another thread.
  // The returned object may be passed across sequences until any of its public
  // methods are called, at which point it becomes bound to the calling
  // sequence.
  std::unique_ptr<Connector> Clone();

  // Returns |true| if this Connector instance is already bound to a thread.
  bool IsBound() const;

  // Binds a Connector receiver to the other end of this Connector.
  void BindConnectorReceiver(mojo::PendingReceiver<mojom::Connector> receiver);

  base::WeakPtr<Connector> GetWeakPtr();

  // Test-only methods for interception of requests on Connectors. Consider
  // using TestConnectorFactory to create and inject fake Connectors into
  // production code instead of using these methods to monkey-patch existing
  // Connector objects.
  void OverrideBinderForTesting(const service_manager::ServiceFilter& filter,
                                const std::string& interface_name,
                                const TestApi::Binder& binder);
  bool HasBinderOverrideForTesting(const service_manager::ServiceFilter& filter,
                                   const std::string& interface_name);
  void ClearBinderOverrideForTesting(
      const service_manager::ServiceFilter& filter,
      const std::string& interface_name);
  void ClearBinderOverridesForTesting();

 private:
  using BinderOverrideMap = std::map<std::string, TestApi::Binder>;

  void OnConnectionError();
  bool BindConnectorIfNecessary();

  mojo::PendingRemote<mojom::Connector> unbound_state_;
  mojo::Remote<mojom::Connector> connector_;

  SEQUENCE_CHECKER(sequence_checker_);

  std::map<service_manager::ServiceFilter, BinderOverrideMap>
      local_binder_overrides_;

  base::WeakPtrFactory<Connector> weak_factory_{this};
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CONNECTOR_H_
