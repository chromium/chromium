// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_MANIFEST_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_MANIFEST_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace service_manager {

// Represents metadata about a service that the Service Manager needs in order
// to start and control instances of that given service. This data is provided
// to the Service Manager at initialization time for every known service in the
// system.
//
// A service will typically define a dedicated manifest target in their public
// C++ client library with a single GetManifest() function that returns a
// const ref to a function-local static Manifest. Then any Service Manager
// embedder can reference the manifest in order to include support for that
// service within its runtime environment.
//
// Instead of constructing a Manifest manually, prefer to use a ManifestBuilder
// defined in manifest_builder.h for more readable and maintainable manifest
// definitions.
struct COMPONENT_EXPORT(SERVICE_MANAGER_CPP) Manifest {
 public:
  using ServiceName = std::string;
  using CapabilityName = std::string;
  using InterfaceName = std::string;
  using InterfaceNameSet = std::set<InterfaceName>;
  using CapabilityNameSet = std::set<CapabilityName>;
  using ExposedCapabilityMap = std::map<CapabilityName, InterfaceNameSet>;
  using RequiredCapabilityMap = std::map<ServiceName, CapabilityNameSet>;

  // Represents the display name of this service (in e.g. a task manager).
  struct COMPONENT_EXPORT(SERVICE_MANAGER_CPP) DisplayName {
    enum class Type { kDefault, kRawString, kResourceId };

    DisplayName() : type(Type::kDefault) {}
    explicit DisplayName(const char* raw_string)
        : type(Type::kRawString), raw_string(raw_string) {}
    explicit DisplayName(int resource_id)
        : type(Type::kResourceId), resource_id(resource_id) {}

    Type type;
    union {
      const char* raw_string;
      int resource_id;
    };
  };

  enum class InstanceSharingPolicy {
    // Instances of the service are never shared across groups or instance IDs.
    // Every tuple of group and instance ID corresponds to a unique instance.
    kNoSharing,

    // Only one instance of the service can exist at a time, and any combination
    // of group and instance ID refers to the same single instance. i.e. group
    // and instance ID are effectively ignored when locating the instance of
    // this service on behalf of clients.
    kSingleton,

    // At most one instance of this service will exist with a given instance ID.
    // i.e., instance group is effectively ignored when locating an instance of
    // the service on behalf of a client.
    kSharedAcrossGroups,
  };

  // Indicates how instances of this service are launched. Ignored if this
  // manifest is packaged within another manifest, as launch is always delegated
  // to some instance of the packaging service in that case. See
  // |packaged_services| below for more information about packaged service
  // manifests.
  enum class ExecutionMode {
    // The service implementation is built into the Service Manager embedder's
    // binary (for example Chromium, or any Content embedder), and the embedder
    // handles requests for new instances of the service in-process via
    // ServiceManager::Delegate::RunBuiltinServiceInstanceInCurrentProcess().
    //
    // If a service uses this ExecutionMode in Chromium for example, that means
    // the service always runs in the browser process.
    kInProcessBuiltin,

    // The service implementation is built into the Service Manager embedder's
    // binary (for example Chromium, or any Content embedder), and the embedder
    // handles requests for new instances of the service via
    // ServiceProcess::Delegate::RunService(). The service will always run in
    // a child process sandboxed according to sandbox::mojom::Sandbox (see
    // Options below).
    kOutOfProcessBuiltin,

    // The service is launched out-of-process from a standalone service
    // executable on disk within the running application's directory. The name
    // of the executable is expected to be "${service_name}.service" (or
    // "${service_name}.service.exe" on Windows).
    //
    // Proper sandboxing is currently not supported for standalone service
    // executables, so sandbox::mojom::Sandbox (see Options below) is
    // ignored. This renders
    // standalone service executables generally unsuitable for production
    // environments.
    kStandaloneExecutable,
  };

  // Miscellanous options which control how the service is launched and how it
  // can interact with other service instances in the system.
  struct COMPONENT_EXPORT(SERVICE_MANAGER_CPP) Options {
    Options();
    Options(const Options&);
    Options(Options&&);
    ~Options();

    Options& operator=(Options&&);
    Options& operator=(const Options&);

    // Indicates how instances of this service may be shared across clients.
    InstanceSharingPolicy instance_sharing_policy =
        InstanceSharingPolicy::kNoSharing;

    // If |true|, this service is allowed to connect to other service instances
    // in instance groups other than its own. This is considered a privileged
    // capability, as instance grouping provides natural boundaries for service
    // instance isolation.
    bool can_connect_to_instances_in_any_group = false;

    // If |true|, this service is allowed to connect to other services instances
    // with a specific instance ID. This is considered a privileged capability
    // since it allows this service to instigate the creation of an arbitrary
    // number of service instances.
    bool can_connect_to_instances_with_any_id = false;

    // If |true|, this service is allowed to directly register new service
    // instances with the Service Manager. This is considered a privileged
    // capability since it grants this service a significant degree of control
    // over the entire system's behavior. For example, the service could
    // completely replace other system services and therefore intercept requests
    // intended for those services.
    bool can_register_other_service_instances = false;

    // Indicates how instances of this service are launched. Ignored iff this
    // manifest is packaged within another service's manifest.
    ExecutionMode execution_mode = ExecutionMode::kInProcessBuiltin;

    // The type of sandboxing required by instances of this service. Only used
    // if |execution_mode| is |kOutOfProcessBuiltin| or
    // |kStandaloneExecutable|.
    //
    // TODO(crbug.com/40606841): Make this field a
    // sandbox::mojom::Sandbox enum.
    std::string sandbox_type{"utility"};
  };

  // Represents a file required by instances of the service despite being
  // inaccessible to the service directly, due to e.g. sandboxing constraints.
  //
  // Currently ignored on platforms other than Android and Linux.
  struct PreloadedFileInfo {
    // A key which can be used by the service implementation to locate the
    // file's open descriptor via |base::FileDescriptorStore|.
    std::string key;

    // The path to the file. On Linux this is relative to the main Service
    // Manager embedder's executable (i.e. relative to base::DIR_EXE.) On
    // Android it's an APK asset path.
    base::FilePath path;
  };

  // A helper for Manifest writers to create a set of interfaces to be used in
  // in exposed capabilities.
  template <typename... InterfaceTypes>
  struct InterfaceList {};

  Manifest();
  Manifest(const Manifest&);
  Manifest(Manifest&&);

  ~Manifest();

  Manifest& operator=(const Manifest&);
  Manifest& operator=(Manifest&&);

  // Amends this Manifest with a subset of |other|. Namely, exposed and required
  // capabilities, exposed and required interface filter capabilities, packaged
  // services, and preloaded files are all added from |other| if present.
  Manifest& Amend(Manifest other);

  ServiceName service_name;
  DisplayName display_name;
  Options options;

  // All capabilities exposed by this service. The key is the name of the
  // capability, which is an arbitrary string value chosen by and scoped to the
  // service. The value is a set of mojom interface names, conveying the set of
  // interfaces to which this capability grants access via the Service Manager.
  // See |required_capabilities| for information on how another service can have
  // that access granted to them.
  ExposedCapabilityMap exposed_capabilities;

  // All capabilities required by this service. The key is the name of another
  // service, and the corresponding value is the set of (names of) capabilities
  // required from that service.
  //
  // If a service A declares in its manifest that it requires a capability X
  // from service B, then A will be allowed to request any interface exposed
  // through X (i.e. through the capability in the |exposed_capabilities| field
  // of B's manifest), using |BindInterface()| on A's Connector.
  RequiredCapabilityMap required_capabilities;

  // DEPRECATED: This will be removed soon. Don't add new uses of interface
  // filters. Instead prefer to define explicit broker interfaces and expose
  // them through |exposed_capabilities|.
  //
  // Services may define capabilities to be scoped within a named interface
  // filter. These capabilities do not apply to normal interface binding
  // requests (i.e. requests made by clients through |Connector.BindInterface|).
  // Instead, the exposing service may use |Connector.FilterInterfaces| to
  // set up an InterfaceProvider pipe proxied through the Service Manager. The
  // Service Manager will filter interface requests on that pipe according to
  // the given filter name and remote service name. The remote service must in
  // turn require one or more capabilities from the named filter in order to
  // access any interfaces via the proxied InterfaceProvider, which the exposing
  // service must pass to the remote service somehow.
  //
  // If this all sounds very confusing, that's because it is very confusing.
  // Hence the "DEPRECATED" bit.
  using FilterName = std::string;
  std::map<FilterName, ExposedCapabilityMap>
      exposed_interface_filter_capabilities;

  // DEPRECATED: This will be removed soon. Don't add new uses of interface
  // filters.
  //
  // This is like |required_capabilities|, except that it only grants the
  // requiring/ service access to a set of interfaces on a specific
  // InterfaceProvider, filtered by the exposing service according to an
  // |exposed_interface_filter_capabilities| in that service's manifest. See
  // notes on that field above.
  std::map<FilterName, RequiredCapabilityMap>
      required_interface_filter_capabilities;

  // A list of manifests for services "packaged" by this service. For a service
  // Y to be packaged within a service X means that the Service Manager will
  // always delegate creation of Y instances to an instance of X via calls to
  // |Service::CreatePackagedServiceInstance()|.
  //
  // See
  // https://chromium.googlesource.com/chromium/src/+/main/services/service_manager/README.md#Packaging
  // for more information.
  std::vector<Manifest> packaged_services;
  std::vector<PreloadedFileInfo> preloaded_files;

  // The list of interfaces that this service are allowed to connect to
  // unconditionally on any service.
  InterfaceNameSet interfaces_bindable_on_any_service;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_MANIFEST_H_
