// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_NETWORK_ERROR_URL_LOADER_H_
#define CONTENT_BROWSER_WEBUI_NETWORK_ERROR_URL_LOADER_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace network {
struct ResourceRequest;
}

namespace content {

// Creates the data for chrome://network-error.
void StartNetworkErrorsURLLoader(
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client);

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_NETWORK_ERROR_URL_LOADER_H_
