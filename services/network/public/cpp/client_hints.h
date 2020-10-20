// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CLIENT_HINTS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CLIENT_HINTS_H_

#include <stddef.h>
#include <string>

#include "base/component_export.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"

namespace network {

// Mapping from WebClientHintsType to the hint's name in Accept-CH header.
// The ordering matches the ordering of enums in
// services/network/public/mojom/web_client_hints_types.mojom
COMPONENT_EXPORT(NETWORK_CPP)
extern const char* const kClientHintsNameMapping[];
COMPONENT_EXPORT(NETWORK_CPP) extern const size_t kClientHintsMappingsCount;

// Tries to parse an Accept-CH header. Returns base::nullopt if parsing
// failed and the header should be ignored; otherwise returns a (possibly
// empty) list of hints to accept.
base::Optional<std::vector<network::mojom::WebClientHintsType>>
    COMPONENT_EXPORT(NETWORK_CPP)
        ParseClientHintsHeader(const std::string& header);

// Tries to parse Accept-CH-Lifetime. Returns base::TimeDelta() if unsuccessful.
base::TimeDelta COMPONENT_EXPORT(NETWORK_CPP)
    ParseAcceptCHLifetime(const std::string& header);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CLIENT_HINTS_H_
