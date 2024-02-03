// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SERVICE_INSTANCE_REGISTRY_H_
#define SERVICES_SERVICE_MANAGER_SERVICE_INSTANCE_REGISTRY_H_

#include <map>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/token.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/service_filter.h"

namespace service_manager {

class ServiceInstance;

// A container of (unowned) pointers to ServiceInstance objects, indexed by each
// instance's Identity according to the service's specified instance sharing
// policy.
//
// For example, some services may declare in their manifest that they're
// singletons, in which case only one instance of that service is ever indexed
// at a time.
//
// Other services may declare that they're shared across multiple instance
// groups, but can still have multiple instances identified by unique instance
// IDs.
//
// These different sharing configurations affect how a ServiceFilter is resolved
// to a specific instance by the ServiceManager, and the logic which determines
// that resolution process is encapsulated within ServiceInstanceRegistry.
class ServiceInstanceRegistry {
 public:
  ServiceInstanceRegistry();

  ServiceInstanceRegistry(const ServiceInstanceRegistry&) = delete;
  ServiceInstanceRegistry& operator=(const ServiceInstanceRegistry&) = delete;

  ~ServiceInstanceRegistry();

  // Registers |instance| with the registry. |instance| is not owned by the
  // registry and must be unregistered (see Unregister below) before it's
  // destroyed.
  void Register(ServiceInstance* instance);

  // Unregisters |instance| from the registry if registered.
  bool Unregister(ServiceInstance* instance);

  // Attempts to locate a service instance registered with the registry, using
  // |filter| as matching criteria.
  ServiceInstance* FindMatching(const ServiceFilter& filter) const;

 private:
  // An entry in any of the mappings owned by this object.
  struct Entry {
    Entry(const base::Token& guid, ServiceInstance* instance);
    Entry(const Entry&);
    ~Entry();

    base::Token guid;
    raw_ptr<ServiceInstance> instance = nullptr;
  };

  struct RegularInstanceKey {
    RegularInstanceKey(const std::string& service_name,
                       const base::Token& instance_group,
                       const base::Token& instance_id);
    RegularInstanceKey(const RegularInstanceKey&);
    ~RegularInstanceKey();

    bool operator==(const RegularInstanceKey& other) const;
    bool operator<(const RegularInstanceKey& other) const;

    const std::string service_name;
    const base::Token instance_group;
    const base::Token instance_id;
  };

  struct SharedInstanceKey {
    SharedInstanceKey(const std::string& service_name,
                      const base::Token& instance_id);
    SharedInstanceKey(const SharedInstanceKey&);
    ~SharedInstanceKey();

    bool operator==(const SharedInstanceKey& other) const;
    bool operator<(const SharedInstanceKey& other) const;

    const std::string service_name;
    const base::Token instance_id;
  };

  // Maps a 3-tuple of (service name, instance group, instance ID) to a list of
  // service instances and their GUIDs.
  using RegularInstanceMap = std::map<RegularInstanceKey, std::vector<Entry>>;

  // Maps a 2-tuple of (service name, instance ID) to a list of shared service
  // instances and their GUIDs.
  using SharedInstanceMap = std::map<SharedInstanceKey, std::vector<Entry>>;

  // Maps a service name to a list of singleton instances and their GUIDs.
  using SingletonInstanceMap = std::map<std::string, std::vector<Entry>>;

  ServiceInstance* FindMatchInEntries(
      const std::vector<Entry>& entries,
      const std::optional<base::Token>& guid) const;
  bool EraseEntry(const base::Token& guid, std::vector<Entry>* entries);

  RegularInstanceMap regular_instances_;
  SharedInstanceMap shared_instances_;
  SingletonInstanceMap singleton_instances_;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SERVICE_INSTANCE_REGISTRY_H_
