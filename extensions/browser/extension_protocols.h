// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_PROTOCOLS_H_
#define EXTENSIONS_BROWSER_EXTENSION_PROTOCOLS_H_

#include "base/functional/callback.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace net {
class HttpResponseHeaders;
}

namespace extensions {

using ExtensionProtocolTestHandler =
    base::RepeatingCallback<void(base::FilePath* directory_path,
                                 base::FilePath* relative_path)>;

// Allows tests to set a special handler for chrome-extension:// urls. Note
// that this goes through all the normal security checks; it's essentially a
// way to map extra resources to be included in extensions.
void SetExtensionProtocolTestHandler(ExtensionProtocolTestHandler* handler);

// Creates a new network::mojom::URLLoaderFactory implementation suitable for
// handling navigation requests to extension URLs.
mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreateExtensionNavigationURLLoaderFactory(
    content::BrowserContext* browser_context,
    bool is_web_view_request);

// Creates a new network::mojom::URLLoaderFactory implementation suitable for
// handling dedicated/shared worker main script requests initiated by the
// browser process to extension URLs.
mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreateExtensionWorkerMainResourceURLLoaderFactory(
    content::BrowserContext* browser_context);

// Creates a new network::mojom::URLLoaderFactory implementation suitable for
// handling service worker main/imported script requests initiated by the
// browser process to extension URLs when PlzServiceWorker is enabled or during
// service worker update check.
mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreateExtensionServiceWorkerScriptURLLoaderFactory(
    content::BrowserContext* browser_context);

// Creates a network::mojom::URLLoaderFactory implementation suitable for
// handling subresource requests for extension URLs for the frame identified by
// |render_process_id| and |render_frame_id|.
// This function can also be used to make a factory for other non-subresource
// requests to extension URLs, such as for the service worker script when
// starting a service worker. In that case, render_frame_id will be
// MSG_ROUTING_NONE.
mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreateExtensionURLLoaderFactory(int render_process_id, int render_frame_id);

void EnsureExtensionURLLoaderFactoryShutdownNotifierFactoryBuilt();

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_PROTOCOLS_H_
