// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_UI_URL_LOADER_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_WEB_UI_URL_LOADER_FACTORY_H_

#include <string>

#include "base/containers/flat_set.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {
class RenderFrameHost;
class BrowserContext;
// Create and bind a URLLoaderFactory for loading resources matching the
// specified |scheme| and also from a "pseudo host" matching one in
// |allowed_hosts|.
//
// The factory will only create loaders for requests with the same scheme as
// |scheme|. This is needed because there is more than one scheme used for
// WebUI, and not all have WebUI bindings.
CONTENT_EXPORT
mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreateWebUIURLLoaderFactory(RenderFrameHost* render_frame_host,
                            const std::string& scheme,
                            base::flat_set<std::string> allowed_hosts);

CONTENT_EXPORT
mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreateWebUIServiceWorkerLoaderFactory(
    BrowserContext* browser_context,
    const std::string& scheme,
    base::flat_set<std::string> allowed_hosts);
}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_UI_URL_LOADER_FACTORY_H_
