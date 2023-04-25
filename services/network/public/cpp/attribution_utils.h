// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_UTILS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_UTILS_H_

#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "services/network/public/mojom/attribution.mojom-forward.h"

namespace network {

// Returns the value to be set for `Attribution-Reporting-Support` request
// header.
COMPONENT_EXPORT(NETWORK_CPP)
base::StringPiece GetAttributionSupportHeader(mojom::AttributionSupport);

// Returns whether OS-level attribution is supported.
COMPONENT_EXPORT(NETWORK_CPP)
bool HasAttributionOsSupport(mojom::AttributionSupport);

#if BUILDFLAG(IS_ANDROID)

// Returns whether web attribution is supported.
COMPONENT_EXPORT(NETWORK_CPP)
bool HasAttributionWebSupport(mojom::AttributionSupport);

// Returns whether either web or OS-level attribution is supported.
COMPONENT_EXPORT(NETWORK_CPP)
bool HasAttributionSupport(mojom::AttributionSupport);

#endif

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_UTILS_H_
