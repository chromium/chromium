// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SERVICE_MANAGER_H_
#define SERVICES_SERVICE_MANAGER_SERVICE_MANAGER_H_

#include <memory>
#include <set>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process.h"
#include "base/token.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "sandbox/policy/sandbox_type.h"
#include "services/service_manager/catalog.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_receiver.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/service_manager/service_instance_registry.h"
#include "services/service_manager/service_process_host.h"

namespace service_manager {

class ServiceInstance;

class ServiceManager : public Service {
 public:
  // This is an interface a ServiceManager instance can use to delegate certain
  // operations (like launching in-process services) to its embedding runtime
  // environment.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Asks for a new concrete instance of the service identified by |identity|
    // to be created in the calling (i.e. the Service Manager's) process. If
    // created, the instance should bind to |receiver|. Returns |true| if the
    // instance was created or |false| otherwise (e.g. the service was unknown
    // or in-process instances are not supported by the runtime environment).
    //
    // This is only called for services with the ExecutionMode
    // |kInProcessBuiltin|.
    virtual bool RunBuiltinServiceInstanceInCurrentProcess(
        const Identity& identity,
        mojo::PendingReceiver<mojom::Service> receiver) = 0;

    // Creates a new ServiceProcessHost to host an out-of-process service
    // instance for a service using ExecuteMode |kOutOfProcessBuiltin|.
    //
    // May return null if builtin out-of-process services are not supported by
    // the runtime environment.
    //
    // TODO(crbug.com/40598251): Process launching should be fully the
    // responsibility of the Service Manager. This exists because much of the
    // Chromium process launching logic today is still buried in the Content
    // layer.
    virtual std::unique_ptr<ServiceProcessHost>
    CreateProcessHostForBuiltinServiceInstance(const Identity& identity) = 0;

    // Creates a new ServiceProcessHost to host an out-of-process service
    // instance for a service using a standalone executable (i.e. ExecuteMode in
    // the manifest is |kStandaloneExecutable|).
    //
    // May return null if service executables are not supported by the runtime
    // environment.
    //
    // TODO(crbug.com/40598251): Process launching should be fully the
    // responsibility of the Service Manager. This exists because much of the
    // Chromium process launching logic today is still buried in the Content
    // layer.
    virtual std::unique_ptr<ServiceProcessHost>
    CreateProcessHostForServiceExecutable(
        const base::FilePath& executable_path) = 0;
  };

  // Indicates whether standalone service executables are supported by this
  // ServiceManager instance. Only used when an explicit Delegate is not
  // specified at construction time.
  enum class ServiceExecutablePolicy {
    kSupported,
    kNotSupported,
  };

  // Constructs a new ServiceManager instance. |delegate| is used to augment
  // default Service Manager behavior.
  //
  // |manifests| is the complete list of manifests for all services available to
  // the runtime environment.
  ServiceManager(const std::vector<Manifest>& manifests,
                 std::unique_ptr<Delegate> delegate);

  // Like above but uses a default internal Delegate implementation. With the
  // default implementation, only packaged services, manually registered service
  // instances, or (policy permitting) service executables are supported. No
  // builtin (in-process or out-of-process) services are supported unless
  // manually registered with |RegisterService()| below.
  ServiceManager(const std::vector<Manifest>& manifests,
                 ServiceExecutablePolicy service_executable_policy);

  ServiceManager(const ServiceManager&) = delete;
  ServiceManager& operator=(const ServiceManager&) = delete;

  ~ServiceManager() override;

  // Directly requests that the Service Manager start a new instance for
  // |service_name| if one is not already running.
  //
  // TODO(crbug.com/40601935): Remove this method.
  void StartService(const std::string& service_name);

  // Creates a service instance for |identity|. This is intended for use by the
  // Service Manager's embedder to register instances directly, without
  // requiring a Connector.
  //
  // |metadata_receiver| may be null, in which case the Service Manager assumes
  // the new service is running in the calling process.
  //
  // Returns |true| if registration succeeded, or |false| otherwise.
  bool RegisterService(
      const Identity& identity,
      mojo::PendingRemote<mojom::Service> service,
      mojo::PendingReceiver<mojom::ProcessMetadata> metadata_receiver);

  // Determine information about |service_name| from its manifests. Returns
  // false if the identity does not have a catalog entry.
  bool QueryCatalog(const std::string& service_name,
                    const base::Token& instance_group,
                    std::string* sandbox_type);

  // Attempts to locate a ServiceInstance as a target for a connection request
  // from |source_instance| by matching against |partial_target_filter|. If a
  // suitable instance exists it is returned, otherwise the Service Manager
  // attempts to create a new suitable instance.
  //
  // Returns null if a matching instance did not exist and could not be created,
  // otherwise returns a valid ServiceInstance which matches
  // |partial_target_filter| from |source_instance|'s perspective.
  ServiceInstance* FindOrCreateMatchingTargetInstance(
      const ServiceInstance& source_instance,
      const ServiceFilter& partial_target_filter);

 private:
  friend class ServiceInstance;

  // Erases |instance| from the instance registry. Following this call it is
  // impossible for any call to GetExistingInstance() to return |instance| even
  // though the instance may continue to exist and send requests to the Service
  // Manager.
  void MakeInstanceUnreachable(ServiceInstance* instance);

  // Called when |instance| no longer has any connections to the remote service
  // instance, or when some other fatal error is encountered in managing the
  // instance. Deletes |instance|.
  void DestroyInstance(ServiceInstance* instance);

  // Called by a ServiceInstance as it's being destroyed.
  void OnInstanceStopped(const Identity& identity);

  // Returns a running instance identified by |identity|.
  ServiceInstance* GetExistingInstance(const Identity& identity) const;

  void NotifyServiceCreated(const ServiceInstance& instance);
  void NotifyServiceStarted(const Identity& identity, base::ProcessId pid);
  void NotifyServiceFailedToStart(const Identity& identity);

  void NotifyServicePIDReceived(const Identity& identity, base::ProcessId pid);

  ServiceInstance* CreateServiceInstance(const Identity& identity,
                                         const Manifest& manifest);

  // Called from the instance implementing mojom::ServiceManager.
  void AddListener(mojo::PendingRemote<mojom::ServiceManagerListener> listener);

  // Service:
  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle receiving_pipe) override;

  const std::unique_ptr<Delegate> delegate_;

  ServiceReceiver service_receiver_{this};

  // Ownership of all ServiceInstances.
  using InstanceMap =
      std::set<std::unique_ptr<ServiceInstance>, base::UniquePtrComparator>;
  InstanceMap instances_;

  Catalog catalog_;

  // Maps service identities to reachable instances, allowing for lookup of
  // running instances by ServiceFilter.
  ServiceInstanceRegistry instance_registry_;

  // Always points to the ServiceManager's own Instance. Note that this
  // ServiceInstance still has an entry in |instances_|.
  raw_ptr<ServiceInstance, DanglingUntriaged> service_manager_instance_;

  mojo::RemoteSet<mojom::ServiceManagerListener> listeners_;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SERVICE_MANAGER_H_
