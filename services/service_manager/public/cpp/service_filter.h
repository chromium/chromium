// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_FILTER_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_FILTER_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/token.h"
#include "services/service_manager/public/cpp/identity.h"

namespace service_manager {

// A ServiceFilter is used with APIs like |Connector::BindInterface()| to
// indicate to the Service Manager which service instance(s) a request is
// intended for. Generally a ServiceFilter is used by the Service Manager to
// match against the Identity of each running service instance in order to
// perform some operation on that instance.
class COMPONENT_EXPORT(SERVICE_MANAGER_CPP_TYPES) ServiceFilter {
 public:
  ServiceFilter();
  ServiceFilter(const ServiceFilter&);
  ServiceFilter(ServiceFilter&&);

  // TODO(crbug.com/41424827): Remove this constructor.
  //
  // NOTE: This is intentionally an implicit constructor to avoid a high volume
  // of global churn as |Connector.BindInterface()| is migrated from taking an
  // Identity to taking a ServiceFilter.
  ServiceFilter(const Identity& identity);

  ~ServiceFilter();

  ServiceFilter& operator=(const ServiceFilter& other);

  // Returns a ServiceFilter which only filters by service name. When locating a
  // service instance using such a filter, the Service Manager will consider an
  // instance to be a potential match as long as its service name matches
  // |service_name|. Other constraints may still apply, e.g. for many target
  // services the Service Manager will limit its search space to instances
  // within the caller's own instance group.
  static ServiceFilter ByName(const std::string& service_name);

  // Returns a new ServiceFilter which only matches service instances with the
  // service name |service_name| running with instance ID |instance_id|.
  // Note that callers filtering by instance ID may require special privileges
  // depending on the operation for which the filter is being used.
  //
  // For example, |Connector::BindInterface()| cannot filter by instance ID
  // unless the calling service has
  // |can_connect_to_other_services_with_any_instance_name| option set in its
  // manifest.
  static ServiceFilter ByNameWithId(const std::string& service_name,
                                    const base::Token& instance_id);

  // Returns a new ServiceFilter which only matches service instances with the
  // the service name |service_name| running in instance group |instance_group|.
  // Note that many operations or target services do not allow for cross-group
  // requests and so the Service Manager will ignore such requests when using
  // this kind of filter unless |instance_group| matches the caller's own (in
  // which case using |ByName()| above would be equivalent).
  static ServiceFilter ByNameInGroup(const std::string& service_name,
                                     const base::Token& instance_group);

  // Returns a new ServiceFilter which only matches service instances with the
  // service name |service_name| running in instance group |instance_group| with
  // instance ID |instance_id|. The same caveats which apply to |ByNameWithId()|
  // and |ByNameInGroup()| apply to these filters as well.
  static ServiceFilter ByNameWithIdInGroup(const std::string& service_name,
                                           const base::Token& instance_id,
                                           const base::Token& instance_group);

  // Returns a new ServiceFilter which matches a specific, singular service
  // instance in the system. All fields of |identity| must be valid, including
  // the globally unique ID which identifies a unique service instances across
  // time and space. If that specific service is still running, this filter can
  // match it (subject to restrictions discussed above); otherwise this filter
  // does not match any instance.
  static ServiceFilter ForExactIdentity(const Identity& identity);

  const std::string& service_name() const { return service_name_; }
  void set_service_name(const std::string& service_name) {
    service_name_ = service_name;
  }

  const std::optional<base::Token>& instance_group() const {
    return instance_group_;
  }

  void set_instance_group(const std::optional<base::Token>& instance_group) {
    instance_group_ = instance_group;
  }

  const std::optional<base::Token>& instance_id() const { return instance_id_; }

  void set_instance_id(const std::optional<base::Token>& instance_id) {
    instance_id_ = instance_id;
  }

  const std::optional<base::Token>& globally_unique_id() const {
    return globally_unique_id_;
  }

  void set_globally_unique_id(
      const std::optional<base::Token>& globally_unique_id) {
    globally_unique_id_ = globally_unique_id;
  }

  bool operator<(const ServiceFilter& other) const;

 private:
  ServiceFilter(const std::string& service_name,
                const std::optional<base::Token>& instance_group,
                const std::optional<base::Token>& instance_id,
                const std::optional<base::Token>& globally_unique_id);

  std::string service_name_;
  std::optional<base::Token> instance_group_;
  std::optional<base::Token> instance_id_;
  std::optional<base::Token> globally_unique_id_;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_FILTER_H_
