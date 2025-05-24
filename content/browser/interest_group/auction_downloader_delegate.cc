// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_downloader_delegate.h"

#include <memory>
#include <optional>
#include <string>

#include "base/check.h"
#include "base/types/pass_key.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/interest_group/protected_audience_network_util.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/services/auction_worklet/public/cpp/auction_downloader.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {

AuctionDownloaderDelegate::AuctionDownloaderDelegate(
    base::PassKey<AuctionDownloaderDelegate>,
    FrameTreeNodeId frame_tree_node_id)
    : frame_tree_node_id_(frame_tree_node_id) {}

AuctionDownloaderDelegate::~AuctionDownloaderDelegate() = default;

// static
std::unique_ptr<AuctionDownloaderDelegate>
AuctionDownloaderDelegate::MaybeCreate(FrameTreeNodeId frame_tree_node_id) {
  if (!frame_tree_node_id) {
    return nullptr;
  }
  return std::make_unique<AuctionDownloaderDelegate>(
      base::PassKey<AuctionDownloaderDelegate>(), frame_tree_node_id);
}

void AuctionDownloaderDelegate::OnNetworkSendRequest(
    network::ResourceRequest& request) {
  // This should only be invoked once, since only one request is being made,
  // without any retries.
  DCHECK(!request_id_);
  request_id_ = request.devtools_request_id;

  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  if (frame_tree_node) {
    std::optional<std::string> user_agent_override =
        GetUserAgentOverrideForProtectedAudience(frame_tree_node);
    if (user_agent_override) {
      request.headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                                std::move(user_agent_override).value());
    }

    SetUpDevtoolsForRequest(frame_tree_node, request);
  }

  devtools_instrumentation::OnAuctionWorkletNetworkRequestWillBeSent(
      frame_tree_node_id_, request, base::TimeTicks::Now());
}

void AuctionDownloaderDelegate::OnNetworkResponseReceived(
    const GURL& url,
    const network::mojom::URLResponseHead& head) {
  devtools_instrumentation::OnAuctionWorkletNetworkResponseReceived(
      frame_tree_node_id_, *request_id_, /*loader_id=*/*request_id_, url, head);
}

void AuctionDownloaderDelegate::OnNetworkRequestComplete(
    const network::URLLoaderCompletionStatus& status) {
  devtools_instrumentation::OnAuctionWorkletNetworkRequestComplete(
      frame_tree_node_id_, *request_id_, status);
}

}  // namespace content
