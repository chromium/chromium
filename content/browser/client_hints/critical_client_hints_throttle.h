// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CLIENT_HINTS_CRITICAL_CLIENT_HINTS_THROTTLE_H_
#define CONTENT_BROWSER_CLIENT_HINTS_CRITICAL_CLIENT_HINTS_THROTTLE_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CriticalCHRestart {
  kNavigationStarted = 0,
  kHeaderPresent = 1,
  kNavigationRestarted = 2,
  kMaxValue = kNavigationRestarted,
};

}  // namespace

namespace content {

class BrowserContext;
class ClientHintsControllerDelegate;

class CriticalClientHintsThrottle : public blink::URLLoaderThrottle {
 public:
  CriticalClientHintsThrottle(
      BrowserContext* context,
      ClientHintsControllerDelegate* client_hint_delegate,
      FrameTreeNodeId frame_tree_node_id);
  ~CriticalClientHintsThrottle() override;

  // blink::URLLoaderThrottle
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void BeforeWillProcessResponse(
      const GURL& response_url,
      const network::mojom::URLResponseHead& response_head,
      RestartWithURLReset* restart_with_url_reset) override;
  void BeforeWillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      RestartWithURLReset* restart_with_url_reset,
      std::vector<std::string>* to_be_removed_request_headers,
      net::HttpRequestHeaders* modified_request_headers,
      net::HttpRequestHeaders* modified_cors_exempt_request_headers) override;

 private:
  // Contains the logic for whether or not the navigation should restart, and
  // persists the Accept-CH header if there is a restart.
  void MaybeRestartWithHints(
      const network::mojom::URLResponseHead& response_head,
      RestartWithURLReset* restart_with_url_reset);

  raw_ptr<BrowserContext> context_;
  raw_ptr<ClientHintsControllerDelegate> client_hint_delegate_;
  FrameTreeNodeId frame_tree_node_id_;

  // Ensure that there's only one restart per origin
  base::flat_set<url::Origin> restarted_origins_;

  // Url of the last request made.
  GURL response_url_;

  // Headers from the initial request. This should include headers added from an
  // ACCEPT_CH frame that aren't in storage.
  net::HttpRequestHeaders initial_request_headers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CLIENT_HINTS_CRITICAL_CLIENT_HINTS_THROTTLE_H_
