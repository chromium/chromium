// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_CONTEXT_GETTER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NETWORK_CONTEXT_GETTER_H_

#include "base/functional/callback.h"

namespace network {

namespace mojom {
class NetworkContext;
}  // namespace mojom

using NetworkContextGetter =
    base::RepeatingCallback<network::mojom::NetworkContext*(void)>;

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NETWORK_CONTEXT_GETTER_H_
