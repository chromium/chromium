// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_INTEGRITY_POLICY_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_INTEGRITY_POLICY_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/network/public/cpp/integrity_policy.h"
#include "services/network/public/mojom/integrity_policy.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_INTEGRITY_POLICY)
    StructTraits<network::mojom::IntegrityPolicyDataView,
                 network::IntegrityPolicy> {
  using IntegrityPolicy = network::IntegrityPolicy;

  static const std::vector<network::mojom::IntegrityPolicy_Destination>&
  blocked_destinations(const IntegrityPolicy& policy) {
    return policy.blocked_destinations;
  }
  static const std::vector<network::mojom::IntegrityPolicy_Source>& sources(
      const IntegrityPolicy& policy) {
    return policy.sources;
  }
  static const std::vector<std::string>& endpoints(
      const IntegrityPolicy& policy) {
    return policy.endpoints;
  }
  static const std::vector<std::string>& parsing_errors(
      const IntegrityPolicy& policy) {
    return policy.parsing_errors;
  }
  static bool Read(network::mojom::IntegrityPolicyDataView data,
                   network::IntegrityPolicy* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_INTEGRITY_POLICY_MOJOM_TRAITS_H_
