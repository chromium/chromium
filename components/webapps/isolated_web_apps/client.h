// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_CLIENT_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_CLIENT_H_

#include <string>
#include <vector>

#include "base/types/expected.h"
#include "base/version.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "components/webapps/isolated_web_apps/types/url_loading_types.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace web_app {

class IwaRuntimeDataProvider;

// This singleton acts as a bridge between the browser-independent IWA layer and
// the embedder layer (i.e. Chrome).
class IwaClient {
 public:
  struct SourceRequestError {
    net::Error net_error;
    std::string error_description;
  };

  IwaClient(const IwaClient&) = delete;
  IwaClient& operator=(const IwaClient&) = delete;

  static IwaClient* GetInstance();

  // Tells the embedder (who manages the app system) to run the supplied
  // `callback` once all windows of the app defined by `web_bundle_id` are
  // closed.
  virtual void RunWhenAppCloses(
      content::BrowserContext* browser_context,
      const web_package::SignedWebBundleId& web_bundle_id,
      base::OnceClosure callback) = 0;

  // Attempts to look up the correct source (bundle of proxy) for the given
  // `web_bundle_id` and `request.url` (it's guaranteed that `request.url`
  // corresponds to `web_bundle_id`); might return unexpected if there's no app
  // installed or if the embedder doesn't consider it trusted. The embedder
  // might also choose to provide a generated response instead of a source.
  virtual void GetIwaSourceForRequest(
      content::BrowserContext* browser_context,
      const web_package::SignedWebBundleId& web_bundle_id,
      const network::ResourceRequest& request,
      const std::optional<content::FrameTreeNodeId>& frame_tree_node,
      base::OnceCallback<
          void(base::expected<IwaSourceWithModeOrGeneratedResponse,
                              SourceRequestError>)> callback) = 0;

  virtual IwaRuntimeDataProvider* GetRuntimeDataProvider() = 0;

 protected:
  IwaClient();
  virtual ~IwaClient();
};

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_CLIENT_H_
