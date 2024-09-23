// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_NETWORK_CONTEXT_FACTORY_H_
#define DEVICE_FIDO_NETWORK_CONTEXT_FACTORY_H_

#include "base/functional/callback_forward.h"

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

namespace device {
using NetworkContextFactory =
    base::RepeatingCallback<network::mojom::NetworkContext*()>;
}  // namespace device

#endif  // DEVICE_FIDO_NETWORK_CONTEXT_FACTORY_H_
