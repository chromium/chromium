// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_NETWORK_SERVICE_DEVTOOLS_OBSERVER_H_
#define CONTENT_BROWSER_DEVTOOLS_NETWORK_SERVICE_DEVTOOLS_OBSERVER_H_

#include <string>

#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"

namespace content {

class DevToolsAgentHostImpl;
class FrameTreeNode;

// A springboard class to be able to bind to the network service as a
// DevToolsObserver but not requiring the creation of a DevToolsAgentHostImpl.
class NetworkServiceDevToolsObserver : public network::mojom::DevToolsObserver {
 public:
  NetworkServiceDevToolsObserver(
      base::PassKey<NetworkServiceDevToolsObserver> pass_key,
      const std::string& devtools_agent_id,
      FrameTreeNodeId frame_tree_node_id);
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
      const base::TimeTicks timestamp,
      network::mojom::ClientSecurityStatePtr security_state,
      network::mojom::OtherPartitionInfoPtr other_partition_info) override;
  void OnRawResponse(
      const std::string& devtools_request_id,
      const net::CookieAndLineAccessResultList& response_cookie_list,
      std::vector<network::mojom::HttpRawHeaderPairPtr> response_headers,
      const std::optional<std::string>& response_headers_text,
      network::mojom::IPAddressSpace resource_address_space,
      int32_t http_status_code,
      const std::optional<net::CookiePartitionKey>& cookie_partition_key)
      override;
  void OnEarlyHintsResponse(
      const std::string& devtools_request_id,
      std::vector<network::mojom::HttpRawHeaderPairPtr> headers) override;
  void OnTrustTokenOperationDone(
      const std::string& devtools_request_id,
      network::mojom::TrustTokenOperationResultPtr result) override;
  void OnPrivateNetworkRequest(
      const std::optional<std::string>& devtools_request_id,
      const GURL& url,
      bool is_warning,
      network::mojom::IPAddressSpace resource_address_space,
      network::mojom::ClientSecurityStatePtr client_security_state) override;
  void OnCorsPreflightRequest(
      const base::UnguessableToken& devtools_request_id,
      const net::HttpRequestHeaders& request_headers,
      network::mojom::URLRequestDevToolsInfoPtr request_info,
      const GURL& initiator_url,
      const std::string& initiator_devtools_request_id) override;
  void OnCorsPreflightResponse(
      const base::UnguessableToken& devtools_request_id,
      const GURL& url,
      network::mojom::URLResponseHeadDevToolsInfoPtr head) override;
  void OnCorsPreflightRequestCompleted(
      const base::UnguessableToken& devtools_request_id,
      const network::URLLoaderCompletionStatus& status) override;
  void OnCorsError(const std::optional<std::string>& devtool_request_id,
                   const std::optional<::url::Origin>& initiator_origin,
                   network::mojom::ClientSecurityStatePtr client_security_state,
                   const GURL& url,
                   const network::CorsErrorStatus& status,
                   bool is_warning) override;
  void OnOrbError(const std::optional<std::string>& devtools_request_id,
                  const GURL& url) override;
  void OnSubresourceWebBundleMetadata(const std::string& devtools_request_id,
                                      const std::vector<GURL>& urls) override;
  void OnSubresourceWebBundleMetadataError(
      const std::string& devtools_request_id,
      const std::string& error_message) override;
  void OnSubresourceWebBundleInnerResponse(
      const std::string& inner_request_devtools_id,
      const GURL& url,
      const std::optional<std::string>& bundle_request_devtools_id) override;
  void OnSubresourceWebBundleInnerResponseError(
      const std::string& inner_request_devtools_id,
      const GURL& url,
      const std::string& error_message,
      const std::optional<std::string>& bundle_request_devtools_id) override;
  void OnSharedDictionaryError(
      const std::string& devtool_request_id,
      const GURL& url,
      network::mojom::SharedDictionaryError error) override;
  void Clone(mojo::PendingReceiver<network::mojom::DevToolsObserver> listener)
      override;

  DevToolsAgentHostImpl* GetDevToolsAgentHost();

  // This will be set for devtools observers that are created with a worker
  // context, empty otherwise.
  const std::string devtools_agent_id_;

  // This will be set for devtools observers that are created with a frame
  // context, otherwise it will be unset.
  const FrameTreeNodeId frame_tree_node_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_NETWORK_SERVICE_DEVTOOLS_OBSERVER_H_
