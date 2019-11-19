// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/service_manager.h"

#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/token.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/public/mojom/service_control.mojom.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "services/service_manager/service_instance.h"
#include "services/service_manager/service_process_host.h"

#if !defined(OS_IOS)
#include "services/service_manager/service_process_launcher.h"
#endif

namespace service_manager {

namespace {

const char kCapability_ServiceManager[] = "service_manager:service_manager";

#if defined(OS_WIN)
const char kServiceExecutableExtension[] = ".service.exe";
#elif !defined(OS_IOS)
const char kServiceExecutableExtension[] = ".service";
#endif

base::ProcessId GetCurrentPid() {
#if defined(OS_IOS)
  // iOS does not support base::Process.
  return 0;
#else
  return base::Process::Current().Pid();
#endif
}

const Identity& GetServiceManagerInstanceIdentity() {
  static base::NoDestructor<Identity> id{service_manager::mojom::kServiceName,
                                         kSystemInstanceGroup, base::Token{},
                                         base::Token::CreateRandom()};
  return *id;
}

// Default ServiceProcessHost implementation. This launches a service process
// from a standalone executable only.
class DefaultServiceProcessHost : public ServiceProcessHost {
 public:
  explicit DefaultServiceProcessHost(const base::FilePath& executable_path)
#if !defined(OS_IOS)
      : launcher_(nullptr, executable_path)
#endif
  {
  }

  ~DefaultServiceProcessHost() override = default;

  mojo::PendingRemote<mojom::Service> Launch(const Identity& identity,
                                             SandboxType sandbox_type,
                                             const base::string16& display_name,
                                             LaunchCallback callback) override {
#if defined(OS_IOS)
    return mojo::NullRemote();
#else
    // TODO(https://crbug.com/781334): Support sandboxing.
    CHECK_EQ(sandbox_type, SANDBOX_TYPE_NO_SANDBOX);
    return launcher_
        .Start(identity, SANDBOX_TYPE_NO_SANDBOX, std::move(callback))
        .PassInterface();
#endif  // defined(OS_IOS)
  }

 private:
#if !defined(OS_IOS)
  ServiceProcessLauncher launcher_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DefaultServiceProcessHost);
};

// Default ServiceManager::Delegate implementation. This supports launching only
// standalone service executables.
//
// TODO(https://crbug.com/781334): Migrate all service process support into this
// implementation and merge it into ServiceProcessHost.
class DefaultServiceManagerDelegate : public ServiceManager::Delegate {
 public:
  explicit DefaultServiceManagerDelegate(
      ServiceManager::ServiceExecutablePolicy service_executable_policy)
      : service_executable_policy_(service_executable_policy) {}
  ~DefaultServiceManagerDelegate() override = default;

  bool RunBuiltinServiceInstanceInCurrentProcess(
      const Identity& identity,
      mojo::PendingReceiver<mojom::Service> receiver) override {
    return false;
  }

  std::unique_ptr<ServiceProcessHost>
  CreateProcessHostForBuiltinServiceInstance(
      const Identity& identity) override {
    return nullptr;
  }

  std::unique_ptr<ServiceProcessHost> CreateProcessHostForServiceExecutable(
      const base::FilePath& executable_path) override {
    if (service_executable_policy_ ==
        ServiceManager::ServiceExecutablePolicy::kNotSupported) {
      return nullptr;
    }

    DCHECK_EQ(ServiceManager::ServiceExecutablePolicy::kSupported,
              service_executable_policy_);
    return std::make_unique<DefaultServiceProcessHost>(executable_path);
  }

 private:
  const ServiceManager::ServiceExecutablePolicy service_executable_policy_;

  DISALLOW_COPY_AND_ASSIGN(DefaultServiceManagerDelegate);
};

}  // namespace

ServiceManager::ServiceManager(const std::vector<Manifest>& manifests,
                               std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)), catalog_(manifests) {
  Manifest service_manager_manifest =
      ManifestBuilder()
          .WithOptions(ManifestOptionsBuilder()
                           .WithInstanceSharingPolicy(
                               Manifest::InstanceSharingPolicy::kSingleton)
                           .Build())
          .ExposeCapability(kCapability_ServiceManager,
                            Manifest::InterfaceList<mojom::ServiceManager>())
          .Build();
  service_manager_instance_ = CreateServiceInstance(
      GetServiceManagerInstanceIdentity(), service_manager_manifest);
  service_manager_instance_->SetPID(GetCurrentPid());

  mojo::PendingRemote<mojom::Service> remote;
  service_binding_.Bind(remote.InitWithNewPipeAndPassReceiver());
  service_manager_instance_->StartWithRemote(std::move(remote));
}

ServiceManager::ServiceManager(
    const std::vector<Manifest>& manifests,
    ServiceExecutablePolicy service_executable_policy)
    : ServiceManager(manifests,
                     std::make_unique<DefaultServiceManagerDelegate>(
                         service_executable_policy)) {}

ServiceManager::~ServiceManager() {
  // Stop all of the instances before destroying any of them. This ensures that
  // all Services will receive connection errors and avoids possible deadlock in
  // the case where one ServiceInstance's destructor blocks waiting for its
  // Runner to quit, while that Runner's corresponding Service blocks its
  // shutdown on a distinct Service receiving a connection error.
  //
  // Also ensure we tear down the ServiceManager instance last. This is to avoid
  // hitting bindings DCHECKs, since the ServiceManager or Catalog may at any
  // given time own in-flight responders for Instances' Connector requests.
  for (auto& instance : instances_) {
    if (instance.get() != service_manager_instance_)
      instance->Stop();
  }
  service_manager_instance_->Stop();
  instances_.clear();
}

void ServiceManager::SetInstanceQuitCallback(
    base::OnceCallback<void(const Identity&)> callback) {
  instance_quit_callback_ = std::move(callback);
}

ServiceInstance* ServiceManager::FindOrCreateMatchingTargetInstance(
    const ServiceInstance& source_instance,
    const ServiceFilter& partial_target_filter) {
  TRACE_EVENT_INSTANT1("service_manager", "ServiceManager::Connect",
                       TRACE_EVENT_SCOPE_THREAD, "original_name",
                       partial_target_filter.service_name());
  if (partial_target_filter.service_name() == mojom::kServiceName)
    return service_manager_instance_;

  const service_manager::Manifest* manifest =
      catalog_.GetManifest(partial_target_filter.service_name());
  if (!manifest) {
    LOG(ERROR) << "Failed to resolve service name: "
               << partial_target_filter.service_name();
    return nullptr;
  }

  service_manager::ServiceFilter target_filter = partial_target_filter;
  if (!target_filter.instance_group()) {
    // Inherit the source instance's group if none was specified by the
    // caller's provided filter.
    target_filter.set_instance_group(
        source_instance.identity().instance_group());
  }
  if (!target_filter.instance_id()) {
    // Assume the default (zero) instance ID if none was specified by the
    // caller's provided filter.
    target_filter.set_instance_id(base::Token());
  }

  // Use an existing instance if possible.
  ServiceInstance* target_instance =
      instance_registry_.FindMatching(target_filter);
  if (target_instance)
    return target_instance;

  // If there was no existing instance but the caller is requesting a specific
  // globally unique ID for the target, ignore the request. That instance is
  // obviously no longer running, and globally unique IDs are never reused.
  if (target_filter.globally_unique_id())
    return nullptr;

  const service_manager::Manifest* parent_manifest =
      catalog_.GetParentManifest(manifest->service_name);

  // New instances to be shared globally or across instance groups are assigned
  // their own random instance group. Packaged service instances also retain
  // the target filter group regardless of sharing policy.
  const base::Token target_group =
      manifest->options.instance_sharing_policy ==
                  Manifest::InstanceSharingPolicy::kNoSharing ||
              parent_manifest
          ? *target_filter.instance_group()
          : base::Token::CreateRandom();

  // New singleton instances are always forced to instance ID zero.
  const base::Token target_instance_id =
      manifest->options.instance_sharing_policy ==
              Manifest::InstanceSharingPolicy::kSingleton
          ? base::Token()
          : *target_filter.instance_id();

  DCHECK(!target_instance);
  target_instance = CreateServiceInstance(
      Identity(target_filter.service_name(), target_group, target_instance_id,
               target_filter.globally_unique_id().value_or(
                   base::Token::CreateRandom())),
      *manifest);

  if (parent_manifest) {
    // This service is provided by another service, as indicated by the
    // providing service's manifest. Get an instance of that service first, and
    // ask it to create an instance of this one.
    auto factory_filter = ServiceFilter::ByNameWithIdInGroup(
        parent_manifest->service_name, *target_filter.instance_id(),
        *target_filter.instance_group());
    ServiceInstance* factory_instance = FindOrCreateMatchingTargetInstance(
        *service_manager_instance_, factory_filter);

    mojo::PendingRemote<mojom::ProcessMetadata> metadata;
    auto metadata_receiver = metadata.InitWithNewPipeAndPassReceiver();
    mojo::PendingRemote<mojom::Service> remote;
    bool created =
        factory_instance &&
        factory_instance->CreatePackagedServiceInstance(
            target_instance->identity(),
            remote.InitWithNewPipeAndPassReceiver(), std::move(metadata));
    if (!created) {
      DestroyInstance(target_instance);
      return nullptr;
    }

    target_instance->BindProcessMetadataReceiver(std::move(metadata_receiver));
    target_instance->StartWithRemote(std::move(remote));
    return target_instance;
  }

  switch (manifest->options.execution_mode) {
    case Manifest::ExecutionMode::kInProcessBuiltin: {
      mojo::PendingRemote<mojom::Service> remote;
      if (!delegate_->RunBuiltinServiceInstanceInCurrentProcess(
              target_instance->identity(),
              remote.InitWithNewPipeAndPassReceiver())) {
        DestroyInstance(target_instance);
        return nullptr;
      }
      target_instance->StartWithRemote(std::move(remote));
      break;
    }

#if !defined(OS_IOS)
    case Manifest::ExecutionMode::kOutOfProcessBuiltin: {
      auto process_host = delegate_->CreateProcessHostForBuiltinServiceInstance(
          target_instance->identity());
      if (!process_host ||
          !target_instance->StartWithProcessHost(
              std::move(process_host),
              UtilitySandboxTypeFromString(manifest->options.sandbox_type))) {
        DestroyInstance(target_instance);
        return nullptr;
      }
      break;
    }

    case Manifest::ExecutionMode::kStandaloneExecutable: {
      base::FilePath service_exe_root;
      CHECK(base::PathService::Get(base::DIR_ASSETS, &service_exe_root));
      auto process_host = delegate_->CreateProcessHostForServiceExecutable(
          service_exe_root.AppendASCII(manifest->service_name +
                                       kServiceExecutableExtension));
      if (!process_host ||
          !target_instance->StartWithProcessHost(
              std::move(process_host),
              UtilitySandboxTypeFromString(manifest->options.sandbox_type))) {
        DestroyInstance(target_instance);
        return nullptr;
      }
      break;
    }
#else   // !defined(OS_IOS)
    default:
      NOTREACHED();
      return nullptr;
#endif  // !defined(OS_IOS)
  }

  return target_instance;
}

void ServiceManager::StartService(const std::string& service_name) {
  FindOrCreateMatchingTargetInstance(
      *service_manager_instance_,
      ServiceFilter::ByNameInGroup(service_name, kSystemInstanceGroup));
}

bool ServiceManager::QueryCatalog(const std::string& service_name,
                                  const base::Token& instance_group,
                                  std::string* sandbox_type) {
  const Manifest* manifest = catalog_.GetManifest(service_name);
  if (!manifest)
    return false;
  *sandbox_type = manifest->options.sandbox_type;
  return true;
}

bool ServiceManager::RegisterService(
    const Identity& identity,
    mojo::PendingRemote<mojom::Service> service,
    mojo::PendingReceiver<mojom::ProcessMetadata> metadata_receiver) {
  if (!identity.IsValid())
    return false;

  const service_manager::Manifest* manifest =
      catalog_.GetManifest(identity.name());
  if (!manifest) {
    LOG(ERROR) << "Failed to resolve service name: " << identity.name();
    return false;
  }

  ServiceInstance* instance = CreateServiceInstance(identity, *manifest);
  if (metadata_receiver)
    instance->BindProcessMetadataReceiver(std::move(metadata_receiver));
  else
    instance->SetPID(GetCurrentPid());
  instance->StartWithRemote(std::move(service));
  return true;
}

void ServiceManager::MakeInstanceUnreachable(ServiceInstance* instance) {
  instance_registry_.Unregister(instance);
}

void ServiceManager::DestroyInstance(ServiceInstance* instance) {
  // We never clean up the ServiceManager's own instance.
  if (instance == service_manager_instance_)
    return;

  MakeInstanceUnreachable(instance);
  auto it = instances_.find(instance);
  DCHECK(it != instances_.end());

  // Deletes |instance|.
  instances_.erase(it);
}

void ServiceManager::OnInstanceStopped(const Identity& identity) {
  for (auto& listener : listeners_) {
    listener->OnServiceStopped(identity);
  }

  if (!instance_quit_callback_.is_null())
    std::move(instance_quit_callback_).Run(identity);
}

ServiceInstance* ServiceManager::GetExistingInstance(
    const Identity& identity) const {
  return instance_registry_.FindMatching(
      ServiceFilter::ForExactIdentity(identity));
}

void ServiceManager::NotifyServiceCreated(const ServiceInstance& instance) {
  mojom::RunningServiceInfoPtr info = instance.CreateRunningServiceInfo();
  for (auto& listener : listeners_) {
    listener->OnServiceCreated(info.Clone());
  }
}

void ServiceManager::NotifyServiceStarted(const Identity& identity,
                                          base::ProcessId pid) {
  for (auto& listener : listeners_) {
    listener->OnServiceStarted(identity, pid);
  }
}

void ServiceManager::NotifyServiceFailedToStart(const Identity& identity) {
  for (auto& listener : listeners_) {
    listener->OnServiceFailedToStart(identity);
  }
}

void ServiceManager::NotifyServicePIDReceived(const Identity& identity,
                                              base::ProcessId pid) {
  for (auto& listener : listeners_) {
    listener->OnServicePIDReceived(identity, pid);
  }
}

ServiceInstance* ServiceManager::CreateServiceInstance(
    const Identity& identity,
    const Manifest& manifest) {
  DCHECK(identity.IsValid());

  auto instance = std::make_unique<ServiceInstance>(this, identity, manifest);
  auto* raw_instance = instance.get();

  instances_.insert(std::move(instance));

  // NOTE: |instance| has been passed elsewhere. Use |raw_instance| from this
  // point forward. It's safe for the extent of this method.

  instance_registry_.Register(raw_instance);

  return raw_instance;
}

void ServiceManager::AddListener(
    mojo::PendingRemote<mojom::ServiceManagerListener> listener) {
  std::vector<mojom::RunningServiceInfoPtr> infos;
  for (auto& instance : instances_)
    infos.push_back(instance->CreateRunningServiceInfo());

  mojo::Remote<mojom::ServiceManagerListener> listener_remote;
  listener_remote.Bind(std::move(listener));
  listener_remote->OnInit(std::move(infos));
  listeners_.Add(std::move(listener_remote));
}

void ServiceManager::OnBindInterface(
    const BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle receiving_pipe) {
  ServiceInstance* instance = GetExistingInstance(source_info.identity);
  DCHECK(instance);
  if (interface_name == mojom::ServiceManager::Name_) {
    instance->BindServiceManagerReceiver(
        mojo::PendingReceiver<mojom::ServiceManager>(
            std::move(receiving_pipe)));
  }
}

}  // namespace service_manager
