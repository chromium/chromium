// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_IDENTITY_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_IDENTITY_H_

#include <string>

#include "base/component_export.h"
#include "base/token.h"

namespace service_manager {

// Represents the identity of a single service instance running in the system.
// Every service instance has an Identity assigned to it, either by the Service
// Manager or by a trusted client using the privileged
// |Connector.RegisterServiceInstance()| API. Each Identity is globally unique
// across space and time.
//
// |name| is the name of the service, as specified in the service's manifest.
//
// |instance_group| is a base::Token representing the identity of an isolated
// group of instances running in the system.
//
// |instance_id| identifies a more specific instance within the instance's
// group, or globally if the service's instances are shared across groups. Note
// that at any given time there is only a single running service instance with
// any given combination of |name|, |instance_group|, and |instance_id|.
//
// |globally_unique_id| is a randomly generated token whose sole purpose is to
// differentiate identities of instances over time where all three other fields
// are identical.
class COMPONENT_EXPORT(SERVICE_MANAGER_CPP_TYPES) Identity {
 public:
  Identity();
  explicit Identity(const std::string& name,
                    const base::Token& instance_group,
                    const base::Token& instance_id,
                    const base::Token& globally_unique_id);
  Identity(const Identity& other);
  ~Identity();

  Identity& operator=(const Identity& other);
  bool operator<(const Identity& other) const;
  bool operator==(const Identity& other) const;
  bool operator!=(const Identity& other) const { return !(*this == other); }

  bool IsValid() const;

  std::string ToString() const;

  const std::string& name() const { return name_; }
  const base::Token& instance_group() const { return instance_group_; }
  const base::Token& instance_id() const { return instance_id_; }
  const base::Token& globally_unique_id() const { return globally_unique_id_; }

 private:
  std::string name_;
  base::Token instance_group_;
  base::Token instance_id_;
  base::Token globally_unique_id_;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_IDENTITY_H_
