// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_PROVIDER_SPEC_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_PROVIDER_SPEC_H_

#include <map>
#include <set>
#include <string>
#include <unordered_map>

#include "services/service_manager/public/cpp/types_export.h"

namespace service_manager {

using Capability = std::string;
using CapabilitySet = std::set<std::string>;
using Interface = std::string;
using InterfaceSet = std::set<std::string>;
using Name = std::string;

// See comments in
// services/service_manager/public/mojom/interface_provider_spec.mojom for
// a description of InterfaceProviderSpec.
struct SERVICE_MANAGER_PUBLIC_CPP_TYPES_EXPORT InterfaceProviderSpec {
  InterfaceProviderSpec();
  InterfaceProviderSpec(const InterfaceProviderSpec& other);
  InterfaceProviderSpec(InterfaceProviderSpec&& other);
  ~InterfaceProviderSpec();
  InterfaceProviderSpec& operator=(const InterfaceProviderSpec& other);
  InterfaceProviderSpec& operator=(InterfaceProviderSpec&& other);
  bool operator==(const InterfaceProviderSpec& other) const;
  bool operator<(const InterfaceProviderSpec& other) const;
  std::map<Capability, InterfaceSet> provides;
  std::map<Name, CapabilitySet> needs;
};

// Map of spec name -> spec.
using InterfaceProviderSpecMap =
    std::unordered_map<std::string, InterfaceProviderSpec>;

// Convenience for reading a spec named |spec_name| out of |map|. If such a spec
// is found, |spec| is modified and this function returns true. If a spec is not
// found, |spec| is unmodified and this function returns false.
bool SERVICE_MANAGER_PUBLIC_CPP_TYPES_EXPORT
GetInterfaceProviderSpec(const std::string& spec_name,
                         const InterfaceProviderSpecMap& map,
                         InterfaceProviderSpec* spec);

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_PROVIDER_SPEC_H_
