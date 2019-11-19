// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SERVICE_INSTANCE_H_
#define SERVICES_SERVICE_MANAGER_SERVICE_INSTANCE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/containers/unique_ptr_adapters.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/public/mojom/service_control.mojom.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/service_manager/sandbox/sandbox_type.h"

namespace service_manager {

class ServiceManager;
class ServiceProcessHost;

// ServiceInstance is the Service Manager-side representation of a service
// instance running in the system. All communication between the Service
// Manager and a running service instance is facilitated by a single
// corresponding ServiceInstance object in the Service Manager, dedicated to
// that instance.
class ServiceInstance : public mojom::Connector,
                        public mojom::ProcessMetadata,
                        public mojom::ServiceControl,
                        public mojom::ServiceManager {
 public:
  // |service_manager| must outlive this ServiceInstance.
  ServiceInstance(service_manager::ServiceManager* service_manager,
                  const Identity& identity,
                  const Manifest& manifest);
  ~ServiceInstance() override;

  const Identity& identity() const { return identity_; }
  const Manifest& manifest() const { return manifest_; }

  // mojom::ProcessMetadata:
  void SetPID(base::ProcessId pid) override;

  // Starts this instance using an already-established Service pipe.
  void StartWithRemote(mojo::PendingRemote<mojom::Service> remote);

#if !defined(OS_IOS)
  // Starts this instance from a path to a service executable on disk.
  bool StartWithProcessHost(std::unique_ptr<ServiceProcessHost> host,
                            SandboxType sandbox_type);
#endif  // !defined(OS_IOS)

  // Binds an endpoint for this instance to receive metadata about its
  // corresponding service process, if any.
  void BindProcessMetadataReceiver(
      mojo::PendingReceiver<mojom::ProcessMetadata> receiver);

  // Forwards a BindInterface request from |source_instance| to this instance,
  // iff it should be allowed based on manifest constraints. Returns |true| if
  // the request was allowed, or |false| otherwise.
  bool MaybeAcceptConnectionRequest(
      const ServiceInstance& source_instance,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle receiving_pipe,
      mojom::BindInterfacePriority priority);

  // Asks this service instance to bind a new concrete service implementation
  // for |packaged_instance_identity|. The packaged instance will always
  // correspond to a service packaged in this service's manifest.
  bool CreatePackagedServiceInstance(
      const Identity& packaged_instance_identity,
      mojo::PendingReceiver<mojom::Service> receiver,
      mojo::PendingRemote<mojom::ProcessMetadata> metadata);

  // Stops receiving any new messages from the service instance and renders the
  // instance permanently unreachable. Note that this does NOT make any attempt
  // to join the service instance's process if any exists.
  void Stop();

  // Creates a structure of metadata describing this instance, to be passed to
  // ServiceManagerListener clients observing running instances in the system.
  mojom::RunningServiceInfoPtr CreateRunningServiceInfo() const;

  // Binds a ServiceManager interface receiver for this instance. Instances may
  // connect to this interface on the Service Manager if sufficiently privileged
  // to observe Service Manager state according to manifest declarations.
  void BindServiceManagerReceiver(
      mojo::PendingReceiver<mojom::ServiceManager> receiver);

 private:
  class InterfaceFilter;
  friend class InterfaceFilter;

  void OnStartCompleted(
      mojo::PendingReceiver<mojom::Connector> connector_receiver,
      mojo::PendingAssociatedReceiver<mojom::ServiceControl> control_receiver);
  void OnConnectRequestAcknowledged();
  void MarkUnreachable();
  void MaybeNotifyPidAvailable();
  void OnServiceDisconnected();
  void OnConnectorDisconnected();
  void HandleServiceOrConnectorDisconnection();

  // Examines an interface connection request coming from this service instance
  // and determines whether it should be allowed to reach any designated target
  // instance. Returns |true| if so, or |false| otherwise.
  //
  // If |target_interface_name| is null, it is sufficient for this (the source)
  // service to have access to *any* arbitrary interface on the target service.
  bool CanConnectToOtherInstance(
      const ServiceFilter& target_filter,
      const base::Optional<std::string>& target_interface_name);

  // mojom::Connector:
  void BindInterface(const ServiceFilter& target_filter,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle receiving_pipe,
                     mojom::BindInterfacePriority priority,
                     BindInterfaceCallback callback) override;
  void QueryService(const std::string& service_name,
                    QueryServiceCallback callback) override;
  void WarmService(const ServiceFilter& target_filter,
                   WarmServiceCallback callback) override;
  void RegisterServiceInstance(
      const Identity& identity,
      mojo::ScopedMessagePipeHandle service_remote_handle,
      mojo::PendingReceiver<mojom::ProcessMetadata> metadata_receiver,
      RegisterServiceInstanceCallback callback) override;
  void Clone(mojo::PendingReceiver<mojom::Connector> receiver) override;

  // mojom::ServiceControl:
  void RequestQuit() override;

  // mojom::ServiceManager:
  void AddListener(
      mojo::PendingRemote<mojom::ServiceManagerListener> listener) override;

  // Always owns |this|.
  service_manager::ServiceManager* const service_manager_;

  // A unique identity for this instance. Distinct from PID, as a single process
  // may host multiple service instances. Globally unique across time and space.
  const Identity identity_;

  // The static service manifest provided for this service at system
  // initialization time.
  const Manifest manifest_;

  // Indicates if this instance is allowed to communicate with all service
  // instances in the system.
  const bool can_contact_all_services_;

#if !defined(OS_IOS)
  std::unique_ptr<ServiceProcessHost> process_host_;
#endif

  // The Service remote used to control the instance.
  mojo::Remote<mojom::Service> service_remote_;

  // Receivers for the various interfaces implemented by this object. These all
  // receive calls directly from the service instance itself or some trusted
  // representative thereof.
  mojo::Receiver<mojom::ProcessMetadata> process_metadata_receiver_{this};
  mojo::ReceiverSet<mojom::Connector> connector_receivers_;
  mojo::ReceiverSet<mojom::ServiceManager> service_manager_receivers_;
  mojo::AssociatedReceiver<mojom::ServiceControl> control_receiver_{this};

  // The PID of the process running the service instance, if known.
  base::ProcessId pid_ = base::kNullProcessId;

  // The current lifecycle state of the service, e.g. whether it's currently
  // starting, running, etc.
  mojom::InstanceState state_ = mojom::InstanceState::kCreated;

  // Indicates if the instance is permanently stopped.
  bool stopped_ = false;

  // The number of outstanding OnBindingInterface requests currently in flight
  // for this instance. This is the total number of OnBindInterface requests
  // sent to the instance, minus the number of acks received so far. If non-zero
  // clean instance termination is impossible at the moment.
  int pending_service_connections_ = 0;

  base::WeakPtrFactory<ServiceInstance> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceInstance);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SERVICE_INSTANCE_H_
