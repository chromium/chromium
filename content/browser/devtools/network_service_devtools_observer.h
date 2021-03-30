// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_NETWORK_SERVICE_DEVTOOLS_OBSERVER_H_
#define CONTENT_BROWSER_DEVTOOLS_NETWORK_SERVICE_DEVTOOLS_OBSERVER_H_

#include <string>

#include "base/types/pass_key.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_request.mojom.h"

namespace content {

class DevToolsAgentHostImpl;

// A springboard class to be able to bind to the network service as a
// DevToolsObserver but not requiring the creation of a DevToolsAgentHostImpl.
class CONTENT_EXPORT NetworkServiceDevToolsObserver
    : public network::mojom::DevToolsObserver {
 public:
  NetworkServiceDevToolsObserver(
      base::PassKey<NetworkServiceDevToolsObserver> pass_key,
      const std::string& devtools_agent_id,
      int frame_tree_node_id);
  ~NetworkServiceDevToolsObserver() override;

  static mojo::PendingRemote<network::mojom::DevToolsObserver> MakeSelfOwned(
      const std::string& id);
  static mojo::PendingRemote<network::mojom::DevToolsObserver> MakeSelfOwned(
      FrameTreeNode* frame_tree_node);

 private:
  // network::mojom::DevToolsObserver overrides.
  void OnRawRequest(
      const std::string& devtools_request_id,
      const net::CookieAccessResultList& request_cookie_list,
      std::vector<network::mojom::HttpRawHeaderPairPtr> request_headers,
      network::mojom::ClientSecurityStatePtr security_state) override;
  void OnRawResponse(
      const std::string& devtools_request_id,
      const net::CookieAndLineAccessResultList& response_cookie_list,
      std::vector<network::mojom::HttpRawHeaderPairPtr> response_headers,
      const base::Optional<std::string>& response_headers_text,
      network::mojom::IPAddressSpace resource_address_space) override;
  void OnTrustTokenOperationDone(
      const std::string& devtools_request_id,
      network::mojom::TrustTokenOperationResultPtr result) override;
  void OnPrivateNetworkRequest(
      const base::Optional<std::string>& devtools_request_id,
      const GURL& url,
      bool is_warning,
      network::mojom::IPAddressSpace resource_address_space,
      network::mojom::ClientSecurityStatePtr client_security_state) override;
  void OnCorsPreflightRequest(
      const base::UnguessableToken& devtools_request_id,
      const network::ResourceRequest& request,
      const GURL& initiator_url,
      const std::string& initiator_devtools_request_id) override;
  void OnCorsPreflightResponse(
      const base::UnguessableToken& devtools_request_id,
      const GURL& url,
      network::mojom::URLResponseHeadPtr head) override;
  void OnCorsPreflightRequestCompleted(
      const base::UnguessableToken& devtools_request_id,
      const network::URLLoaderCompletionStatus& status) override;
  void OnCorsError(const base::Optional<std::string>& devtool_request_id,
                   const base::Optional<::url::Origin>& initiator_origin,
                   const GURL& url,
                   const network::CorsErrorStatus& status) override;
  void Clone(mojo::PendingReceiver<network::mojom::DevToolsObserver> listener)
      override;

  DevToolsAgentHostImpl* GetDevToolsAgentHost();

  // This will be set for devtools observers that are created with a worker
  // context, empty otherwise.
  const std::string devtools_agent_id_;

  // This will be set for devtools observers that are created with a frame
  // context, otherwise it will be kFrameTreeNodeInvalidId.
  const int frame_tree_node_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_NETWORK_SERVICE_DEVTOOLS_OBSERVER_H_
