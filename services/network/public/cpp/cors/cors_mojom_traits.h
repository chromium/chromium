// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_MOJOM_TRAITS_H_

#include <string>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/mojom/cors.mojom-shared.h"

namespace mojo {

template <>
class COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::CorsErrorStatusDataView,
                 network::CorsErrorStatus> {
 public:
  static network::mojom::CorsError cors_error(
      const network::CorsErrorStatus& status) {
    return status.cors_error;
  }

  static const std::string& failed_parameter(
      const network::CorsErrorStatus& status) {
    return status.failed_parameter;
  }

  static network::mojom::IPAddressSpace target_address_space(
      const network::CorsErrorStatus& status) {
    return status.target_address_space;
  }

  static network::mojom::IPAddressSpace resource_address_space(
      const network::CorsErrorStatus& status) {
    return status.resource_address_space;
  }

  static bool has_authorization_covered_by_wildcard_on_preflight(
      const network::CorsErrorStatus& status) {
    return status.has_authorization_covered_by_wildcard_on_preflight;
  }

  static const base::UnguessableToken& issue_id(
      const network::CorsErrorStatus& status) {
    return status.issue_id;
  }

  static bool Read(network::mojom::CorsErrorStatusDataView data,
                   network::CorsErrorStatus* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_MOJOM_TRAITS_H_
