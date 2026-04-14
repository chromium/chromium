// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CT_POLICY_STATUS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CT_POLICY_STATUS_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/cert/ct_policy_status.h"
#include "services/network/public/mojom/ct_policy_status.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    EnumTraits<network::mojom::CTPolicyCompliance,
               net::ct::CTPolicyCompliance> {
  static network::mojom::CTPolicyCompliance ToMojom(
      net::ct::CTPolicyCompliance status);
  static net::ct::CTPolicyCompliance FromMojom(
      network::mojom::CTPolicyCompliance input);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    EnumTraits<network::mojom::CTRequirementsStatus,
               net::ct::CTRequirementsStatus> {
  static network::mojom::CTRequirementsStatus ToMojom(
      net::ct::CTRequirementsStatus status);
  static net::ct::CTRequirementsStatus FromMojom(
      network::mojom::CTRequirementsStatus input);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CT_POLICY_STATUS_MOJOM_TRAITS_H_
