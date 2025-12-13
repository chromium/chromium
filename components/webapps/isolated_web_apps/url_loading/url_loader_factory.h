// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_URL_LOADING_URL_LOADER_FACTORY_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_URL_LOADING_URL_LOADER_FACTORY_H_

#include <optional>

#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace web_app {

// A URLLoaderFactory used for the isolated-app:// scheme.
class IsolatedWebAppURLLoaderFactory {
 public:
  // Returns mojo::PendingRemote to a newly constructed
  // IsolatedWebAppURLLoaderFactory. The factory is self-owned - it will delete
  // itself once there are no more receivers (including the receiver associated
  // with the returned mojo::PendingRemote and the receivers bound by the Clone
  // method).
  //
  // If `app_origin` is present, the factory will only handle requests that are
  // same-origin to it.
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> CreateForFrame(
      content::BrowserContext* browser_context,
      std::optional<url::Origin> app_origin,
      content::FrameTreeNodeId frame_tree_node_id);

  // The same as `CreateForFrame`, but doesn't have access to a FrameTreeNode
  // to log errors to.
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> Create(
      content::BrowserContext* browser_context,
      std::optional<url::Origin> app_origin);

  static void EnsureAssociatedFactoryBuilt();
};

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_URL_LOADING_URL_LOADER_FACTORY_H_
