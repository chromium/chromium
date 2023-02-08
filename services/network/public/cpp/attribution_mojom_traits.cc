// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/attribution_mojom_traits.h"
#include "services/network/public/cpp/trigger_attestation.h"
#include "services/network/public/mojom/attribution.mojom-shared.h"

namespace mojo {

bool StructTraits<network::mojom::TriggerAttestationDataView,
                  network::TriggerAttestation>::
    Read(network::mojom::TriggerAttestationDataView data,
         network::TriggerAttestation* out) {
  std::string token;
  if (!data.ReadToken(&token)) {
    return false;
  }

  std::string aggregatable_report_id;
  if (!data.ReadAggregatableReportId(&aggregatable_report_id)) {
    return false;
  }

  auto trigger_attesation = network::TriggerAttestation::Create(
      std::move(token), aggregatable_report_id);
  if (!trigger_attesation) {
    return false;
  }

  *out = std::move(*trigger_attesation);
  return true;
}

}  // namespace mojo
