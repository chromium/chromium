// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_OPENER_POLICY_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_OPENER_POLICY_MOJOM_TRAITS_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/network/public/cpp/cross_origin_opener_policy.h"
#include "services/network/public/mojom/cross_origin_opener_policy.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::CrossOriginOpenerPolicyDataView,
                 network::CrossOriginOpenerPolicy> {
  static network::mojom::CrossOriginOpenerPolicyValue value(
      const network::CrossOriginOpenerPolicy& coop) {
    return coop.value;
  }
  static const std::optional<std::string>& reporting_endpoint(
      const network::CrossOriginOpenerPolicy& coop) {
    return coop.reporting_endpoint;
  }
  static network::mojom::CrossOriginOpenerPolicyValue report_only_value(
      const network::CrossOriginOpenerPolicy& coop) {
    return coop.report_only_value;
  }
  static const std::optional<std::string>& report_only_reporting_endpoint(
      const network::CrossOriginOpenerPolicy& coop) {
    return coop.report_only_reporting_endpoint;
  }
  static network::mojom::CrossOriginOpenerPolicyValue soap_by_default_value(
      const network::CrossOriginOpenerPolicy& coop) {
    return coop.soap_by_default_value;
  }

  static bool Read(network::mojom::CrossOriginOpenerPolicyDataView view,
                   network::CrossOriginOpenerPolicy* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_OPENER_POLICY_MOJOM_TRAITS_H_
