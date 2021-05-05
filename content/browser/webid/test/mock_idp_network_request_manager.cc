// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/test/mock_idp_network_request_manager.h"

namespace content {

MockIdpNetworkRequestManager::MockIdpNetworkRequestManager(
    const GURL& provider,
    const url::Origin& relying_party)
    : IdpNetworkRequestManager(provider, relying_party, nullptr) {}

MockIdpNetworkRequestManager::~MockIdpNetworkRequestManager() = default;

}  // namespace content
