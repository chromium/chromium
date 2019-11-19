// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/service_instance.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/mojom/constants.mojom.h"
#include "services/service_manager/service_manager.h"
#include "services/service_manager/service_process_host.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_IOS)
#include "services/service_manager/service_process_launcher.h"
#endif  // !defined(OS_IOS)

namespace service_manager {

namespace {

// Returns the set of capabilities required from the target by the source.
std::set<std::string> GetRequiredCapabilities(
    const Manifest::RequiredCapabilityMap& source_requirements,
    const std::string& target_service) {
  std::set<std::string> capabilities;

  // Start by looking for requirements specific to the target identity.
  auto it = source_requirements.find(target_service);
  if (it != source_requirements.end()) {
    std::copy(it->second.begin(), it->second.end(),
              std::inserter(capabilities, capabilities.begin()));
  }

  // Apply wild card rules too.
  it = source_requirements.find("*");
  if (it != source_requirements.end()) {
    std::copy(it->second.begin(), it->second.end(),
              std::inserter(capabilities, capabilities.begin()));
  }
  return capabilities;
}

void ReportBlockedInterface(const Manifest::ServiceName& source_service_name,
                            const Manifest::ServiceName& target_service_name,
                            const std::string& target_interface_name) {
#if DCHECK_IS_ON()
  // While it would not be correct to assert that this never happens (e.g. a
  // compromised process may request invalid interfaces), we do want to
  // effectively treat all occurrences of this branch in production code as
  // bugs that must be fixed. This crash allows such bugs to be caught in
  // testing rather than relying on easily overlooked log messages.
  NOTREACHED()
#else
  LOG(ERROR)
#endif
      << "The Service Manager prevented service \"" << source_service_name
      << "\" from binding interface \"" << target_interface_name << "\""
      << " in target service \"" << target_service_name << "\". You probably "
      << "need to update one or more service manifests to ensure that \""
      << target_service_name << "\" exposes \"" << target_interface_name
      << "\" through a capability and that \"" << source_service_name
      << "\" requires that capability from the \"" << target_service_name
      << "\" service.";
}

void ReportBlockedStartService(const std::string& source_service_name,
                               const std::string& target_service_name) {
#if DCHECK_IS_ON()
  // See the note in ReportBlockedInterface above.
  NOTREACHED()
#else
  LOG(ERROR)
#endif
      << "Service \"" << source_service_name << "\" has attempted to manually "
      << "start service \"" << target_service_name << "\", but it is not "
      << "sufficiently privileged to do so. You probably need to update one or "
      << "services' manifests in order to remedy this situation.";
}

bool AllowsInterface(const Manifest::RequiredCapabilityMap& source_requirements,
                     const std::string& target_name,
                     const Manifest::ExposedCapabilityMap& target_capabilities,
                     const std::string& interface_name) {
  std::set<std::string> allowed_interfaces;
  std::set<std::string> required_capabilities =
      GetRequiredCapabilities(source_requirements, target_name);
  for (const auto& capability : required_capabilities) {
    auto it = target_capabilities.find(capability);
    if (it != target_capabilities.end()) {
      for (const auto& interface_name : it->second)
        allowed_interfaces.insert(interface_name);
    }
  }

  bool allowed =
      allowed_interfaces.count("*") || allowed_interfaces.count(interface_name);
  return allowed;
}

}  // namespace

ServiceInstance::ServiceInstance(
    service_manager::ServiceManager* service_manager,
    const Identity& identity,
    const Manifest& manifest)
    : service_manager_(service_manager),
      identity_(identity),
      manifest_(manifest),
      can_contact_all_services_(manifest_.required_capabilities.count("*") ==
                                1) {
  DCHECK(identity_.IsValid());
}

ServiceInstance::~ServiceInstance() {
  // The instance may have already been stopped prior to destruction if the
  // ServiceManager itself is being torn down.
  if (!stopped_)
    Stop();
}

void ServiceInstance::SetPID(base::ProcessId pid) {
#if !defined(OS_IOS)
  // iOS does not support base::Process and simply passes 0 here, so elide
  // this check on that platform.
  if (pid == base::kNullProcessId) {
    // Destroys |this|.
    service_manager_->DestroyInstance(this);
    return;
  }
#endif
  pid_ = pid;
  MaybeNotifyPidAvailable();
}

void ServiceInstance::StartWithRemote(
    mojo::PendingRemote<mojom::Service> remote) {
  DCHECK(!service_remote_);
  service_remote_.Bind(std::move(remote));
  service_remote_.set_disconnect_handler(base::BindOnce(
      &ServiceInstance::OnServiceDisconnected, base::Unretained(this)));
  service_remote_->OnStart(identity_,
                           base::BindOnce(&ServiceInstance::OnStartCompleted,
                                          base::Unretained(this)));
  service_manager_->NotifyServiceCreated(*this);
}

#if !defined(OS_IOS)
bool ServiceInstance::StartWithProcessHost(
    std::unique_ptr<ServiceProcessHost> host,
    SandboxType sandbox_type) {
  DCHECK(!service_remote_);
  DCHECK(!process_host_);

  base::string16 display_name;
  switch (manifest_.display_name.type) {
    case Manifest::DisplayName::Type::kDefault:
      display_name =
          base::ASCIIToUTF16(base::StrCat({identity_.name(), "service"}));
      break;
    case Manifest::DisplayName::Type::kRawString:
      display_name = base::ASCIIToUTF16(manifest_.display_name.raw_string);
      break;
    case Manifest::DisplayName::Type::kResourceId:
      display_name =
          l10n_util::GetStringUTF16(manifest_.display_name.resource_id);
      break;
  }
  auto remote = host->Launch(
      identity_, sandbox_type, display_name,
      base::BindOnce(&ServiceInstance::SetPID, weak_ptr_factory_.GetWeakPtr()));
  if (!remote)
    return false;

  process_host_ = std::move(host);
  StartWithRemote(std::move(remote));
  return true;
}
#endif  // !defined(OS_IOS)

void ServiceInstance::BindProcessMetadataReceiver(
    mojo::PendingReceiver<mojom::ProcessMetadata> receiver) {
  process_metadata_receiver_.Bind(std::move(receiver));
}

bool ServiceInstance::MaybeAcceptConnectionRequest(
    const ServiceInstance& source_instance,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle receiving_pipe,
    mojom::BindInterfacePriority priority) {
  if (state_ == mojom::InstanceState::kUnreachable)
    return false;

  const Manifest& source_manifest = source_instance.manifest();
  const bool bindable_on_any_service = base::Contains(
      source_manifest.interfaces_bindable_on_any_service, interface_name);
  const bool allowed_by_capabilities =
      AllowsInterface(source_manifest.required_capabilities, identity_.name(),
                      manifest_.exposed_capabilities, interface_name);
  if (!bindable_on_any_service && !allowed_by_capabilities) {
    ReportBlockedInterface(source_instance.identity().name(), identity_.name(),
                           interface_name);
    return false;
  }

  base::OnceClosure on_bind_interface_complete;
  if (priority == mojom::BindInterfacePriority::kImportant) {
    pending_service_connections_++;
    on_bind_interface_complete = base::BindOnce(
        &ServiceInstance::OnConnectRequestAcknowledged, base::Unretained(this));
  }

  service_remote_->OnBindInterface(
      BindSourceInfo(
          source_instance.identity(),
          GetRequiredCapabilities(source_manifest.required_capabilities,
                                  identity_.name())),
      interface_name, std::move(receiving_pipe),
      std::move(on_bind_interface_complete));

  return true;
}

bool ServiceInstance::CreatePackagedServiceInstance(
    const Identity& packaged_instance_identity,
    mojo::PendingReceiver<mojom::Service> receiver,
    mojo::PendingRemote<mojom::ProcessMetadata> metadata) {
  if (!service_remote_)
    return false;
  service_remote_->CreatePackagedServiceInstance(
      packaged_instance_identity, std::move(receiver), std::move(metadata));
  return true;
}

void ServiceInstance::Stop() {
  DCHECK(!stopped_);

  // Shut down all receivers as well as the Service remote. The service should
  // observe disconnection of its corresponding Service receiver and react by
  // self-terminating ASAP.
  service_remote_.reset();
  process_metadata_receiver_.reset();
  connector_receivers_.Clear();
  service_manager_receivers_.Clear();
  MarkUnreachable();

  if (state_ == mojom::InstanceState::kCreated)
    service_manager_->NotifyServiceFailedToStart(identity_);
  else
    service_manager_->OnInstanceStopped(identity_);

  stopped_ = true;
}

mojom::RunningServiceInfoPtr ServiceInstance::CreateRunningServiceInfo() const {
  return mojom::RunningServiceInfo::New(identity_, pid_, state_);
}

void ServiceInstance::BindServiceManagerReceiver(
    mojo::PendingReceiver<mojom::ServiceManager> receiver) {
  service_manager_receivers_.Add(this, std::move(receiver));
}

void ServiceInstance::OnStartCompleted(
    mojo::PendingReceiver<mojom::Connector> connector_receiver,
    mojo::PendingAssociatedReceiver<mojom::ServiceControl> control_receiver) {
  state_ = mojom::InstanceState::kStarted;
  if (connector_receiver.is_valid()) {
    connector_receivers_.Add(this, std::move(connector_receiver));
    connector_receivers_.set_disconnect_handler(base::BindRepeating(
        &ServiceInstance::OnConnectorDisconnected, base::Unretained(this)));
  }
  if (control_receiver.is_valid())
    control_receiver_.Bind(std::move(control_receiver));
  service_manager_->NotifyServiceStarted(identity_, pid_);
  MaybeNotifyPidAvailable();
}

void ServiceInstance::OnConnectRequestAcknowledged() {
  DCHECK_GT(pending_service_connections_, 0);
  pending_service_connections_--;
}

void ServiceInstance::MarkUnreachable() {
  state_ = mojom::InstanceState::kUnreachable;
  service_manager_->MakeInstanceUnreachable(this);
}

void ServiceInstance::MaybeNotifyPidAvailable() {
  // Ensure that we only notify listeners of the PID after notifying them of
  // instance start to ensure consistent ordering of ServiceManagerListener
  // messages pertaining to this instance.
  if (state_ == mojom::InstanceState::kStarted &&
      pid_ != base::kNullProcessId) {
    service_manager_->NotifyServicePIDReceived(identity_, pid_);
  }
}

void ServiceInstance::OnServiceDisconnected() {
  service_remote_.reset();
  HandleServiceOrConnectorDisconnection();
}

void ServiceInstance::OnConnectorDisconnected() {
  HandleServiceOrConnectorDisconnection();
}

void ServiceInstance::HandleServiceOrConnectorDisconnection() {
  // As long as the Service remote is still connected, the instance remains
  // alive and reachable.
  if (service_remote_)
    return;

  if (connector_receivers_.empty()) {
    // No more connections of any kind. Destroys |this|.
    service_manager_->DestroyInstance(this);
  } else {
    // The instance is no longer reachable since it has no Service remote, but
    // we still have active Connectors which may send requests to the Service
    // Manager.
    MarkUnreachable();
  }
}

bool ServiceInstance::CanConnectToOtherInstance(
    const ServiceFilter& target_filter,
    const base::Optional<std::string>& target_interface_name) {
  if (target_filter.service_name().empty()) {
    DLOG(ERROR) << "ServiceFilter has no service name.";
    return false;
  }

  bool skip_instance_group_check =
      manifest_.options.instance_sharing_policy ==
          Manifest::InstanceSharingPolicy::kSingleton ||
      manifest_.options.instance_sharing_policy ==
          Manifest::InstanceSharingPolicy::kSharedAcrossGroups ||
      manifest_.options.can_connect_to_instances_in_any_group;

  if (!skip_instance_group_check && target_filter.instance_group() &&
      target_filter.instance_group() != identity_.instance_group() &&
      target_filter.instance_group() != kSystemInstanceGroup) {
    LOG(ERROR) << "Instance " << identity_.ToString() << " attempting to "
               << "connect to " << target_filter.service_name() << " in "
               << "group " << target_filter.instance_group()->ToString()
               << " without |can_connect_to_instances_in_any_group| set to "
               << "|true|.";
    return false;
  }
  if (target_filter.instance_id() && !target_filter.instance_id()->is_zero() &&
      !manifest_.options.can_connect_to_instances_with_any_id) {
    LOG(ERROR) << "Instance " << identity_.ToString()
               << " attempting to connect to " << target_filter.service_name()
               << " with instance ID "
               << target_filter.instance_id()->ToString() << " without "
               << "|can_connect_to_instances_with_any_id| set to |true|.";
    return false;
  }

  if (can_contact_all_services_ ||
      !manifest_.interfaces_bindable_on_any_service.empty() ||
      manifest_.required_capabilities.find(target_filter.service_name()) !=
          manifest_.required_capabilities.end()) {
    return true;
  }

  if (target_interface_name) {
    ReportBlockedInterface(identity_.name(), target_filter.service_name(),
                           *target_interface_name);
  } else {
    ReportBlockedStartService(identity_.name(), target_filter.service_name());
  }

  return false;
}

void ServiceInstance::BindInterface(
    const ServiceFilter& target_filter,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle receiving_pipe,
    mojom::BindInterfacePriority priority,
    BindInterfaceCallback callback) {
  if (!CanConnectToOtherInstance(target_filter, interface_name)) {
    std::move(callback).Run(mojom::ConnectResult::ACCESS_DENIED, base::nullopt);
    return;
  }

  ServiceInstance* target_instance =
      service_manager_->FindOrCreateMatchingTargetInstance(*this,
                                                           target_filter);
  bool allowed =
      target_instance &&
      target_instance->MaybeAcceptConnectionRequest(
          *this, interface_name, std::move(receiving_pipe), priority);
  if (!allowed) {
    std::move(callback).Run(mojom::ConnectResult::ACCESS_DENIED, base::nullopt);
    return;
  }

  std::move(callback).Run(mojom::ConnectResult::SUCCEEDED,
                          target_instance->identity());
}

void ServiceInstance::QueryService(const std::string& service_name,
                                   QueryServiceCallback callback) {
  std::string sandbox_type;
  const bool success = service_manager_->QueryCatalog(
      service_name, identity_.instance_group(), &sandbox_type);
  if (success)
    std::move(callback).Run(mojom::ServiceInfo::New(sandbox_type));
  else
    std::move(callback).Run(nullptr);
}

void ServiceInstance::WarmService(const ServiceFilter& target_filter,
                                  WarmServiceCallback callback) {
  if (!CanConnectToOtherInstance(target_filter,
                                 base::nullopt /* interface_name */)) {
    std::move(callback).Run(mojom::ConnectResult::ACCESS_DENIED, base::nullopt);
    return;
  }

  ServiceInstance* target_instance =
      service_manager_->FindOrCreateMatchingTargetInstance(*this,
                                                           target_filter);
  if (!target_instance) {
    std::move(callback).Run(mojom::ConnectResult::ACCESS_DENIED, base::nullopt);
    return;
  }

  std::move(callback).Run(mojom::ConnectResult::SUCCEEDED,
                          target_instance->identity());
}

void ServiceInstance::RegisterServiceInstance(
    const Identity& identity,
    mojo::ScopedMessagePipeHandle service_remote_handle,
    mojo::PendingReceiver<mojom::ProcessMetadata> metadata_receiver,
    RegisterServiceInstanceCallback callback) {
  auto target_filter = ServiceFilter::ForExactIdentity(identity);
  if (!CanConnectToOtherInstance(target_filter,
                                 base::nullopt /* interface_name */)) {
    std::move(callback).Run(mojom::ConnectResult::ACCESS_DENIED);
    return;
  }

  mojo::PendingRemote<mojom::Service> service_remote(
      std::move(service_remote_handle), 0);

  if (!manifest_.options.can_register_other_service_instances) {
    LOG(ERROR) << "Instance: " << identity_.name() << " attempting "
               << "to register an instance for a process it created for "
               << "target: " << identity.name() << " without "
               << "the 'can_create_other_service_instances' option.";
    std::move(callback).Run(mojom::ConnectResult::ACCESS_DENIED);
    return;
  }

  if (service_manager_->GetExistingInstance(identity)) {
    LOG(ERROR) << "Instance already exists: " << identity.ToString();
    std::move(callback).Run(mojom::ConnectResult::INVALID_ARGUMENT);
    return;
  }

  if (!service_manager_->RegisterService(identity, std::move(service_remote),
                                         std::move(metadata_receiver))) {
    std::move(callback).Run(mojom::ConnectResult::ACCESS_DENIED);
  }

  std::move(callback).Run(mojom::ConnectResult::SUCCEEDED);
}

void ServiceInstance::Clone(mojo::PendingReceiver<mojom::Connector> receiver) {
  connector_receivers_.Add(this, std::move(receiver));
}

void ServiceInstance::RequestQuit() {
  // Ignore quit requests when there are in-flight connection requests that the
  // instance hasn't acknowledged yet.
  if (pending_service_connections_)
    return;

  // Otherwise behave as if the instance was disconnected.
  OnServiceDisconnected();
}

void ServiceInstance::AddListener(
    mojo::PendingRemote<mojom::ServiceManagerListener> listener) {
  service_manager_->AddListener(std::move(listener));
}

}  // namespace service_manager
