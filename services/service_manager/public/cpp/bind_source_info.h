// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_BIND_SOURCE_INFO_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_BIND_SOURCE_INFO_H_

#include <map>
#include <string>

#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/interface_provider_spec.h"
#include "services/service_manager/public/cpp/types_export.h"

namespace service_manager {

class Identity;

struct SERVICE_MANAGER_PUBLIC_CPP_TYPES_EXPORT BindSourceInfo {
  BindSourceInfo();
  BindSourceInfo(const Identity& identity,
                 const CapabilitySet& required_capabilities);
  BindSourceInfo(const BindSourceInfo& other);
  ~BindSourceInfo();

  Identity identity;
  CapabilitySet required_capabilities;
};

// TODO(https://crbug.com/939141): Rename BindSourceInfo and delete this alias.
using ConnectSourceInfo = BindSourceInfo;

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_BIND_SOURCE_INFO_H_
