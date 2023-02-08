// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/network/public/cpp/trigger_attestation.h"
#include "services/network/public/mojom/attribution.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_ATTRIBUTION_MOJOM_TRAITS)
    StructTraits<network::mojom::TriggerAttestationDataView,
                 network::TriggerAttestation> {
  static const std::string& token(
      const network::TriggerAttestation& attestation) {
    return attestation.token();
  }

  static std::string aggregatable_report_id(
      const network::TriggerAttestation& attestation) {
    return attestation.aggregatable_report_id().AsLowercaseString();
  }

  static bool Read(network::mojom::TriggerAttestationDataView data,
                   network::TriggerAttestation* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_MOJOM_TRAITS_H_
