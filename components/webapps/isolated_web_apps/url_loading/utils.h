// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_URL_LOADING_UTILS_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_URL_LOADING_UTILS_H_

#include <optional>
#include <string>
#include <string_view>

#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace net {
struct MutableNetworkTrafficAnnotationTag;
}  // namespace net

namespace network {
struct ResourceRequest;
}  // namespace network

namespace web_app {

class IwaSourceProxy;

void CompleteWithGeneratedResponse(
    mojo::Remote<network::mojom::URLLoaderClient> loader_client,
    net::HttpStatusCode http_status_code,
    std::optional<std::string> body = std::nullopt,
    std::string_view content_type = "text/html");

void LogErrorMessageToConsole(
    std::optional<content::FrameTreeNodeId> frame_tree_node_id,
    const std::string& error_message);

void LogErrorAndFail(
    const std::string& error_message,
    const std::optional<content::FrameTreeNodeId>& frame_tree_node_id,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client);

void HandleProxy(
    content::BrowserContext* browser_context,
    const web_package::SignedWebBundleId& web_bundle_id,
    const IwaSourceProxy& proxy,
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client,
    const network::ResourceRequest& resource_request,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    const std::optional<content::FrameTreeNodeId>& frame_tree_node_id);

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_URL_LOADING_UTILS_H_
