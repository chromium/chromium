// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_REDUCE_ACCEPT_LANGUAGE_REDUCE_ACCEPT_LANGUAGE_THROTTLE_H_
#define CONTENT_BROWSER_REDUCE_ACCEPT_LANGUAGE_REDUCE_ACCEPT_LANGUAGE_THROTTLE_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace content {

class ReduceAcceptLanguageControllerDelegate;
class OriginTrialsControllerDelegate;

class CONTENT_EXPORT ReduceAcceptLanguageThrottle
    : public blink::URLLoaderThrottle {
 public:
  explicit ReduceAcceptLanguageThrottle(
      ReduceAcceptLanguageControllerDelegate& accept_language_delegate,
      OriginTrialsControllerDelegate* origin_trials_delegate,
      FrameTreeNodeId frame_tree_node_id);
  ~ReduceAcceptLanguageThrottle() override;

  // blink::URLLoaderThrottle
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;

  void BeforeWillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      RestartWithURLReset* restart_with_url_reset,
      std::vector<std::string>* to_be_removed_request_headers,
      net::HttpRequestHeaders* modified_request_headers,
      net::HttpRequestHeaders* modified_cors_exempt_request_headers) override;

  void BeforeWillProcessResponse(
      const GURL& response_url,
      const network::mojom::URLResponseHead& response_head,
      RestartWithURLReset* restart_with_url_reset) override;

 private:
  // Contains the logic for whether or not the navigation should restart, and
  // persists the reduce accept-language if there is a restart.
  void MaybeRestartWithLanguageNegotiation(
      const network::mojom::URLResponseHead& response_head,
      RestartWithURLReset* restart_with_url_reset);

  // The delegate is owned by the BrowserContext, and both are expected to
  // outlive this throttle.
  raw_ref<ReduceAcceptLanguageControllerDelegate> accept_language_delegate_;
  // The delegate is owned by the BrowserContext, and both are expected to
  // outlive this throttle.
  raw_ptr<OriginTrialsControllerDelegate> origin_trials_delegate_;
  FrameTreeNodeId frame_tree_node_id_;

  // Ensure that there's only one restart per origin.
  base::flat_set<url::Origin> restarted_origins_;

  // URL of the last request made.
  GURL last_request_url_;

  // Headers from the initial request. This should include accept-language.
  net::HttpRequestHeaders initial_request_headers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_REDUCE_ACCEPT_LANGUAGE_REDUCE_ACCEPT_LANGUAGE_THROTTLE_H_
