// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AUCTION_DOWNLOADER_DELEGATE_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AUCTION_DOWNLOADER_DELEGATE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/types/pass_key.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/services/auction_worklet/public/cpp/auction_downloader.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

class GURL;

namespace network {
struct ResourceRequest;
struct URLLoaderCompletionStatus;
}  // namespace network

namespace content {

// AuctionDownloader::NetworkEventsDelegate that wires up devtools to a request
// that's initiated by the browser process, and also sets user agent headers, if
// needed. Modifies passed in ResourceRequest to report information to devtools,
// and makes a number of calls into devtools as well. Intended to be used
// unconditionally, regardless of whether devtools is open for a tab or not.
class AuctionDownloaderDelegate
    : public auction_worklet::AuctionDownloader::NetworkEventsDelegate {
 public:
  AuctionDownloaderDelegate(base::PassKey<AuctionDownloaderDelegate>,
                            FrameTreeNodeId frame_tree_node_id);

  ~AuctionDownloaderDelegate() override;

  // Creates an AuctionDownloaderDelegate if `frame_tree_node_id` is
  // non-empty. When the ID is empty, FrameTreeNode::GloballyFindByID() and
  // devtools_instrumentation methods that call it CHECK, so safest to not
  // create an AuctionDownloaderDelegate at all in that case. The ID
  // should typically only be empty in unit tests.
  static std::unique_ptr<AuctionDownloaderDelegate> MaybeCreate(
      FrameTreeNodeId frame_tree_node_id);

  // auction_worklet::AuctionDownloader::NetworkEventsDelegate implementation:
  void OnNetworkSendRequest(network::ResourceRequest& request) override;
  void OnNetworkResponseReceived(
      const GURL& url,
      const network::mojom::URLResponseHead& head) override;
  void OnNetworkRequestComplete(
      const network::URLLoaderCompletionStatus& status) override;

 private:
  // Identifies the FrameTreeNode that triggered the request. Needed for
  // devtools logging.
  const FrameTreeNodeId frame_tree_node_id_;

  // Copied from ResourceRequest passed to OnNetworkSendRequest().
  std::optional<std::string> request_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AUCTION_DOWNLOADER_DELEGATE_H_
