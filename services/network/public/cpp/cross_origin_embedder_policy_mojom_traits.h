// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_EMBEDDER_POLICY_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_EMBEDDER_POLICY_MOJOM_TRAITS_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_CROSS_ORIGIN)
    StructTraits<network::mojom::CrossOriginEmbedderPolicyDataView,
                 network::CrossOriginEmbedderPolicy> {
  static network::mojom::CrossOriginEmbedderPolicyValue value(
      const network::CrossOriginEmbedderPolicy& coep) {
    return coep.value;
  }
  static const std::optional<std::string>& reporting_endpoint(
      const network::CrossOriginEmbedderPolicy& coep) {
    return coep.reporting_endpoint;
  }
  static network::mojom::CrossOriginEmbedderPolicyValue report_only_value(
      const network::CrossOriginEmbedderPolicy& coep) {
    return coep.report_only_value;
  }
  static const std::optional<std::string>& report_only_reporting_endpoint(
      const network::CrossOriginEmbedderPolicy& coep) {
    return coep.report_only_reporting_endpoint;
  }

  static bool Read(network::mojom::CrossOriginEmbedderPolicyDataView view,
                   network::CrossOriginEmbedderPolicy* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_EMBEDDER_POLICY_MOJOM_TRAITS_H_
