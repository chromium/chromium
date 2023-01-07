// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/test/mock_idp_network_request_manager.h"

#include "services/network/public/mojom/client_security_state.mojom.h"

namespace content {

MockIdpNetworkRequestManager::MockIdpNetworkRequestManager()
    : IdpNetworkRequestManager(url::Origin(),
                               nullptr,
                               network::mojom::ClientSecurityState::New()) {}

MockIdpNetworkRequestManager::~MockIdpNetworkRequestManager() = default;

}  // namespace content
