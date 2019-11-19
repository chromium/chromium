// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_REQUEST_MODE_H_
#define SERVICES_NETWORK_PUBLIC_CPP_REQUEST_MODE_H_

#include "base/component_export.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"

namespace network {

// Returns a string corresponding to the |mode| as defined in the spec:
// https://fetch.spec.whatwg.org/#concept-request-mode.
COMPONENT_EXPORT(NETWORK_CPP)
const char* RequestModeToString(network::mojom::RequestMode mode);

COMPONENT_EXPORT(NETWORK_CPP)
bool IsNavigationRequestMode(network::mojom::RequestMode mode);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_REQUEST_MODE_H_
