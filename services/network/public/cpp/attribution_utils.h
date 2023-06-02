// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_UTILS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_UTILS_H_

#include "base/component_export.h"
#include "services/network/public/mojom/attribution.mojom-forward.h"

namespace network {

// Returns whether OS-level attribution is supported.
COMPONENT_EXPORT(NETWORK_CPP)
bool HasAttributionOsSupport(mojom::AttributionSupport);

// Returns whether web attribution is supported.
COMPONENT_EXPORT(NETWORK_CPP)
bool HasAttributionWebSupport(mojom::AttributionSupport);

// Returns whether either web or OS-level attribution is supported.
COMPONENT_EXPORT(NETWORK_CPP)
bool HasAttributionSupport(mojom::AttributionSupport);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_UTILS_H_
