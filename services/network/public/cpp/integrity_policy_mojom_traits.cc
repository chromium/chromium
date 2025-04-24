// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/integrity_policy_mojom_traits.h"

#include <vector>

#include "services/network/public/cpp/integrity_policy.h"

namespace mojo {

// static
bool StructTraits<network::mojom::IntegrityPolicyDataView,
                  network::IntegrityPolicy>::
    Read(network::mojom::IntegrityPolicyDataView data, IntegrityPolicy* out) {
  return data.ReadBlockedDestinations(&out->blocked_destinations) &&
         data.ReadSources(&out->sources) &&
         data.ReadEndpoints(&out->endpoints) &&
         data.ReadParsingErrors(&out->parsing_errors);
}

}  // namespace mojo
