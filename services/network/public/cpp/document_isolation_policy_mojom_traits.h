// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_DOCUMENT_ISOLATION_POLICY_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_DOCUMENT_ISOLATION_POLICY_MOJOM_TRAITS_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/network/public/cpp/document_isolation_policy.h"
#include "services/network/public/mojom/document_isolation_policy.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_DOCUMENT_ISOLATION)
    StructTraits<network::mojom::DocumentIsolationPolicyDataView,
                 network::DocumentIsolationPolicy> {
  static network::mojom::DocumentIsolationPolicyValue value(
      const network::DocumentIsolationPolicy& coep) {
    return coep.value;
  }
  static const std::optional<std::string>& reporting_endpoint(
      const network::DocumentIsolationPolicy& coep) {
    return coep.reporting_endpoint;
  }
  static network::mojom::DocumentIsolationPolicyValue report_only_value(
      const network::DocumentIsolationPolicy& coep) {
    return coep.report_only_value;
  }
  static const std::optional<std::string>& report_only_reporting_endpoint(
      const network::DocumentIsolationPolicy& coep) {
    return coep.report_only_reporting_endpoint;
  }

  static bool Read(network::mojom::DocumentIsolationPolicyDataView view,
                   network::DocumentIsolationPolicy* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_DOCUMENT_ISOLATION_POLICY_MOJOM_TRAITS_H_
