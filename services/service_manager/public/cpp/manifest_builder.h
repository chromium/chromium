// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_MANIFEST_BUILDER_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_MANIFEST_BUILDER_H_

#include <set>
#include <utility>

#include "base/component_export.h"
#include "services/service_manager/public/cpp/manifest.h"

namespace service_manager {

namespace internal {

template <typename InterfaceType>
const char* GetInterfaceName() {
  return InterfaceType::Name_;
}

template <typename... InterfaceTypes>
Manifest::InterfaceNameSet GetInterfaceNames() {
  return Manifest::InterfaceNameSet({GetInterfaceName<InterfaceTypes>()...});
}

}  // namespace internal

// Helper for building Manifest structures in a more readable and writable
// manner than direct construction.
class COMPONENT_EXPORT(SERVICE_MANAGER_CPP) ManifestBuilder {
 public:
  ManifestBuilder();
  ~ManifestBuilder();

  ManifestBuilder& WithServiceName(const char* service_name) {
    manifest_.service_name = service_name;
    return *this;
  }

  ManifestBuilder& WithDisplayName(const char* display_name) {
    manifest_.display_name = Manifest::DisplayName(display_name);
    return *this;
  }

  ManifestBuilder& WithDisplayName(int resource_id) {
    manifest_.display_name = Manifest::DisplayName(resource_id);
    return *this;
  }

  ManifestBuilder& WithOptions(Manifest::Options options) {
    manifest_.options = std::move(options);
    return *this;
  }

  template <typename... InterfaceTypes>
  ManifestBuilder& ExposeCapability(
      const char* name,
      Manifest::InterfaceList<InterfaceTypes...> interfaces) {
    manifest_.exposed_capabilities[name] =
        internal::GetInterfaceNames<InterfaceTypes...>();
    return *this;
  }

  // Prefer the above override. This one exists to support generated code.
  ManifestBuilder& ExposeCapability(const char* name,
                                    std::set<const char*> interface_names) {
    manifest_.exposed_capabilities[name].insert(interface_names.begin(),
                                                interface_names.end());
    return *this;
  }

  ManifestBuilder& RequireCapability(const char* service_name,
                                     const char* capability_name) {
    manifest_.required_capabilities[service_name].insert(capability_name);
    return *this;
  }

  template <typename... InterfaceTypes>
  ManifestBuilder& ExposeInterfaceFilterCapability_Deprecated(
      const char* filter_name,
      const char* capability_name,
      Manifest::InterfaceList<InterfaceTypes...> interfaces) {
    manifest_
        .exposed_interface_filter_capabilities[filter_name][capability_name] =
        internal::GetInterfaceNames<InterfaceTypes...>();
    return *this;
  }

  // Prefer the above override. This one exists to support generated code.
  template <typename... InterfaceTypes>
  ManifestBuilder& ExposeInterfaceFilterCapability_Deprecated(
      const char* filter_name,
      const char* capability_name,
      std::set<const char*> interface_names) {
    manifest_
        .exposed_interface_filter_capabilities[filter_name][capability_name]
        .insert(interface_names.begin(), interface_names.end());
    return *this;
  }

  ManifestBuilder& RequireInterfaceFilterCapability_Deprecated(
      const char* service_name,
      const char* filter_name,
      const char* capability_name) {
    manifest_.required_interface_filter_capabilities[filter_name][service_name]
        .insert(capability_name);
    return *this;
  }

  ManifestBuilder& PackageService(Manifest manifest) {
    manifest_.packaged_services.emplace_back(std::move(manifest));
    return *this;
  }

  ManifestBuilder& PreloadFile(const char* key, const base::FilePath& path) {
    manifest_.preloaded_files.push_back({key, path});
    return *this;
  }

  template <typename... InterfaceTypes>
  ManifestBuilder& WithInterfacesBindableOnAnyService(
      Manifest::InterfaceList<InterfaceTypes...> interfaces) {
    manifest_.interfaces_bindable_on_any_service =
        internal::GetInterfaceNames<InterfaceTypes...>();
    return *this;
  }

  Manifest Build() { return std::move(manifest_); }

 private:
  Manifest manifest_;
};

// Helper for building Manifest::Options structures in a more readable and
// writable manner than direct construction.
class COMPONENT_EXPORT(SERVICE_MANAGER_CPP) ManifestOptionsBuilder {
 public:
  ManifestOptionsBuilder();
  ~ManifestOptionsBuilder();

  ManifestOptionsBuilder& WithInstanceSharingPolicy(
      Manifest::InstanceSharingPolicy policy) {
    options_.instance_sharing_policy = policy;
    return *this;
  }

  ManifestOptionsBuilder& CanConnectToInstancesInAnyGroup(
      bool can_connect_to_instances_in_any_group) {
    options_.can_connect_to_instances_in_any_group =
        can_connect_to_instances_in_any_group;
    return *this;
  }

  ManifestOptionsBuilder& CanConnectToInstancesWithAnyId(
      bool can_connect_to_instances_with_any_id) {
    options_.can_connect_to_instances_with_any_id =
        can_connect_to_instances_with_any_id;
    return *this;
  }

  ManifestOptionsBuilder& CanRegisterOtherServiceInstances(
      bool can_register_other_service_instances) {
    options_.can_register_other_service_instances =
        can_register_other_service_instances;
    return *this;
  }

  ManifestOptionsBuilder& WithExecutionMode(Manifest::ExecutionMode mode) {
    options_.execution_mode = mode;
    return *this;
  }

  ManifestOptionsBuilder& WithSandboxType(const char* sandbox_type) {
    options_.sandbox_type = sandbox_type;
    return *this;
  }

  Manifest::Options Build() { return std::move(options_); }

 private:
  Manifest::Options options_;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_MANIFEST_BUILDER_H_
