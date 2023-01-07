// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_URL_UTIL_H_
#define SERVICES_NETWORK_PUBLIC_CPP_URL_UTIL_H_

#include "base/component_export.h"

class GURL;

namespace network {

// Helper function to determine if `url` refers to a network resource that can
// be loaded by the URLLoaderFactory returned by
// `network::mojom::NetworkContext::CreateURLLoaderFactory` (as opposed to a
// local browser resource like files or blobs).
//
// Note that this is different from `content::IsURLHandledByNetworkStack` which
// determines if starting a request is needed at all (e.g. returning false for
// "about:" or "javascript:" schemes), but considers several non-network schemes
// as handled by the network stack (e.g. returning true for "file:", "blob:",
// "chrome:", etc.).
COMPONENT_EXPORT(NETWORK_CPP)
bool IsURLHandledByNetworkService(const GURL& url);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_URL_UTIL_H_
