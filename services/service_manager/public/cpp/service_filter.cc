// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_filter.h"

#include "base/check.h"

namespace service_manager {

ServiceFilter::ServiceFilter() = default;

ServiceFilter::ServiceFilter(const ServiceFilter&) = default;

ServiceFilter::ServiceFilter(ServiceFilter&&) = default;

ServiceFilter::ServiceFilter(const Identity& identity)
    : service_name_(identity.name()),
      instance_group_(identity.instance_group()),
      instance_id_(identity.instance_id()),
      globally_unique_id_(identity.globally_unique_id()) {}

ServiceFilter::~ServiceFilter() = default;

ServiceFilter& ServiceFilter::operator=(const ServiceFilter& other) = default;

// static
ServiceFilter ServiceFilter::ByName(const std::string& service_name) {
  return ServiceFilter(service_name, std::nullopt /* instance_group */,
                       std::nullopt /* instance_id */,
                       std::nullopt /* globally_unique_id */);
}

// static
ServiceFilter ServiceFilter::ByNameWithId(const std::string& service_name,
                                          const base::Token& instance_id) {
  return ServiceFilter(service_name, std::nullopt /* instance_group */,
                       instance_id, std::nullopt /* globally_unique_id */);
}

// static
ServiceFilter ServiceFilter::ByNameInGroup(const std::string& service_name,
                                           const base::Token& instance_group) {
  return ServiceFilter(service_name, instance_group,
                       std::nullopt /* instance_id */,
                       std::nullopt /* globally_unique_id */);
}

// static
ServiceFilter ServiceFilter::ByNameWithIdInGroup(
    const std::string& service_name,
    const base::Token& instance_id,
    const base::Token& instance_group) {
  return ServiceFilter(service_name, instance_group, instance_id,
                       std::nullopt /* globally_unique_id */);
}

// static
ServiceFilter ServiceFilter::ForExactIdentity(const Identity& identity) {
  DCHECK(identity.IsValid());
  return ServiceFilter(identity.name(), identity.instance_group(),
                       identity.instance_id(), identity.globally_unique_id());
}

bool ServiceFilter::operator<(const ServiceFilter& other) const {
  return std::tie(service_name_, instance_group_, instance_id_,
                  globally_unique_id_) <
         std::tie(other.service_name_, other.instance_group_,
                  other.instance_id_, other.globally_unique_id_);
}

ServiceFilter::ServiceFilter(
    const std::string& service_name,
    const std::optional<base::Token>& instance_group,
    const std::optional<base::Token>& instance_id,
    const std::optional<base::Token>& globally_unique_id)
    : service_name_(service_name),
      instance_group_(instance_group),
      instance_id_(instance_id),
      globally_unique_id_(globally_unique_id) {}

}  // namespace service_manager
