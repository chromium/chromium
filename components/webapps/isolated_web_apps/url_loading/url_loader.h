// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_URL_LOADING_URL_LOADER_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_URL_LOADING_URL_LOADER_H_

#include <optional>

#include "base/files/file_path.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace network {
struct ResourceRequest;
}  // namespace network

namespace web_package {
class SignedWebBundleId;
}  // namespace web_package

namespace web_app {

class IsolatedWebAppURLLoader {
 public:
  static void CreateAndStart(
      content::BrowserContext* browser_context,
      const base::FilePath& web_bundle_path,
      bool dev_mode,
      const web_package::SignedWebBundleId& web_bundle_id,
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client,
      const network::ResourceRequest& resource_request,
      const std::optional<content::FrameTreeNodeId>& frame_tree_node_id);
};

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_URL_LOADING_URL_LOADER_H_
