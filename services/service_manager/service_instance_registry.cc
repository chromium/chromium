// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/service_instance_registry.h"

#include <algorithm>

#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/service_instance.h"

namespace service_manager {

ServiceInstanceRegistry::Entry::Entry(const base::Token& guid,
                                      ServiceInstance* instance)
    : guid(guid), instance(instance) {
  DCHECK(!guid.is_zero());
  DCHECK(instance);
}

ServiceInstanceRegistry::Entry::Entry(const Entry&) = default;

ServiceInstanceRegistry::Entry::~Entry() = default;

ServiceInstanceRegistry::RegularInstanceKey::RegularInstanceKey(
    const std::string& service_name,
    const base::Token& instance_group,
    const base::Token& instance_id)
    : service_name(service_name),
      instance_group(instance_group),
      instance_id(instance_id) {}

ServiceInstanceRegistry::RegularInstanceKey::RegularInstanceKey(
    const RegularInstanceKey&) = default;

ServiceInstanceRegistry::RegularInstanceKey::~RegularInstanceKey() = default;

bool ServiceInstanceRegistry::RegularInstanceKey::operator==(
    const RegularInstanceKey& other) const {
  return service_name == other.service_name &&
         instance_group == other.instance_group &&
         instance_id == other.instance_id;
}

bool ServiceInstanceRegistry::RegularInstanceKey::operator<(
    const RegularInstanceKey& other) const {
  return std::tie(service_name, instance_group, instance_id) <
         std::tie(other.service_name, other.instance_group, other.instance_id);
}

ServiceInstanceRegistry::SharedInstanceKey::SharedInstanceKey(
    const std::string& service_name,
    const base::Token& instance_id)
    : service_name(service_name), instance_id(instance_id) {}

ServiceInstanceRegistry::SharedInstanceKey::SharedInstanceKey(
    const SharedInstanceKey&) = default;

ServiceInstanceRegistry::SharedInstanceKey::~SharedInstanceKey() = default;

bool ServiceInstanceRegistry::SharedInstanceKey::operator==(
    const SharedInstanceKey& other) const {
  return service_name == other.service_name && instance_id == other.instance_id;
}

bool ServiceInstanceRegistry::SharedInstanceKey::operator<(
    const SharedInstanceKey& other) const {
  if (service_name != other.service_name)
    return service_name < other.service_name;
  return instance_id < other.instance_id;
}

ServiceInstanceRegistry::ServiceInstanceRegistry() = default;

ServiceInstanceRegistry::~ServiceInstanceRegistry() = default;

void ServiceInstanceRegistry::Register(ServiceInstance* instance) {
  DCHECK_NE(instance, nullptr);

  const Identity& identity = instance->identity();
  DCHECK_EQ(FindMatching(identity), nullptr);

  switch (instance->manifest().options.instance_sharing_policy) {
    case Manifest::InstanceSharingPolicy::kNoSharing: {
      const RegularInstanceKey key{identity.name(), identity.instance_group(),
                                   identity.instance_id()};
      regular_instances_[key].emplace_back(identity.globally_unique_id(),
                                           instance);
      break;
    }

    case Manifest::InstanceSharingPolicy::kSharedAcrossGroups: {
      const SharedInstanceKey key{identity.name(), identity.instance_id()};
      shared_instances_[key].emplace_back(identity.globally_unique_id(),
                                          instance);
      break;
    }

    case Manifest::InstanceSharingPolicy::kSingleton:
      singleton_instances_[identity.name()].emplace_back(
          identity.globally_unique_id(), instance);
      break;

    default:
      NOTREACHED();
  }
}

bool ServiceInstanceRegistry::Unregister(ServiceInstance* instance) {
  DCHECK(instance);
  const Identity& identity = instance->identity();

  const RegularInstanceKey regular_key{
      identity.name(), identity.instance_group(), identity.instance_id()};
  auto regular_iter = regular_instances_.find(regular_key);
  if (regular_iter != regular_instances_.end()) {
    auto& entries = regular_iter->second;
    if (EraseEntry(identity.globally_unique_id(), &entries)) {
      if (entries.empty())
        regular_instances_.erase(regular_iter);
      return true;
    }
  }

  const SharedInstanceKey shared_key{identity.name(), identity.instance_id()};
  auto shared_iter = shared_instances_.find(shared_key);
  if (shared_iter != shared_instances_.end()) {
    auto& entries = shared_iter->second;
    if (EraseEntry(identity.globally_unique_id(), &entries)) {
      if (entries.empty())
        shared_instances_.erase(shared_iter);
      return true;
    }
  }

  auto singleton_iter = singleton_instances_.find(identity.name());
  if (singleton_iter != singleton_instances_.end()) {
    auto& entries = singleton_iter->second;
    if (EraseEntry(identity.globally_unique_id(), &entries)) {
      if (entries.empty())
        singleton_instances_.erase(singleton_iter);
      return true;
    }
  }

  return false;
}

ServiceInstance* ServiceInstanceRegistry::FindMatching(
    const ServiceFilter& filter) const {
  DCHECK(filter.instance_group());
  DCHECK(filter.instance_id());
  DCHECK(!filter.globally_unique_id() ||
         !filter.globally_unique_id()->is_zero());

  const RegularInstanceKey regular_key{
      filter.service_name(), *filter.instance_group(), *filter.instance_id()};
  auto regular_iter = regular_instances_.find(regular_key);
  if (regular_iter != regular_instances_.end()) {
    return FindMatchInEntries(regular_iter->second,
                              filter.globally_unique_id());
  }

  const SharedInstanceKey shared_key{filter.service_name(),
                                     *filter.instance_id()};
  auto shared_iter = shared_instances_.find(
      SharedInstanceKey(filter.service_name(), *filter.instance_id()));
  if (shared_iter != shared_instances_.end()) {
    return FindMatchInEntries(shared_iter->second, filter.globally_unique_id());
  }

  auto singleton_iter = singleton_instances_.find(filter.service_name());
  if (singleton_iter != singleton_instances_.end()) {
    return FindMatchInEntries(singleton_iter->second,
                              filter.globally_unique_id());
  }

  return nullptr;
}

ServiceInstance* ServiceInstanceRegistry::FindMatchInEntries(
    const std::vector<Entry>& entries,
    const base::Optional<base::Token>& guid) const {
  DCHECK(!entries.empty());
  if (!guid.has_value())
    return entries.front().instance;

  for (const auto& entry : entries) {
    if (entry.guid == *guid)
      return entry.instance;
  }

  return nullptr;
}

bool ServiceInstanceRegistry::EraseEntry(const base::Token& guid,
                                         std::vector<Entry>* entries) {
  auto it =
      std::find_if(entries->begin(), entries->end(),
                   [&guid](const Entry& entry) { return entry.guid == guid; });
  if (it == entries->end())
    return false;

  entries->erase(it);
  return true;
}

}  // namespace service_manager
