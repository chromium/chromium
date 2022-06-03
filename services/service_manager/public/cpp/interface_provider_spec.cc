// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/interface_provider_spec.h"

#include <tuple>

#include "base/check.h"

namespace service_manager {

InterfaceProviderSpec::InterfaceProviderSpec() {}
InterfaceProviderSpec::InterfaceProviderSpec(
    const InterfaceProviderSpec& other) = default;
InterfaceProviderSpec::InterfaceProviderSpec(InterfaceProviderSpec&& other) =
    default;
InterfaceProviderSpec::~InterfaceProviderSpec() {}

InterfaceProviderSpec& InterfaceProviderSpec::operator=(
    const InterfaceProviderSpec& other) = default;
InterfaceProviderSpec& InterfaceProviderSpec::operator=(
    InterfaceProviderSpec&& other) = default;

bool InterfaceProviderSpec::operator==(
    const InterfaceProviderSpec& other) const {
  return other.provides == provides && other.requires == requires;
}

bool InterfaceProviderSpec::operator<(
    const InterfaceProviderSpec& other) const {
  return std::tie(provides, requires) <
         std::tie(other.provides, other.requires);
}

bool GetInterfaceProviderSpec(const std::string& spec_name,
                              const InterfaceProviderSpecMap& map,
                              InterfaceProviderSpec* spec) {
  DCHECK(spec);
  auto it = map.find(spec_name);
  if (it != map.end()) {
    *spec = it->second;
    return true;
  }
  return false;
}

}  // namespace service_manager
