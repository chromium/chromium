// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ORIGIN_TRIALS_CRITICAL_ORIGIN_TRIALS_THROTTLE_H_
#define CONTENT_BROWSER_ORIGIN_TRIALS_CRITICAL_ORIGIN_TRIALS_THROTTLE_H_

#include <optional>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "content/common/content_export.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/gurl.h"

namespace content {

class OriginTrialsControllerDelegate;

class CONTENT_EXPORT CriticalOriginTrialsThrottle
    : public blink::URLLoaderThrottle {
 public:
  // Create a throttle using the passed |origin_trials_delegate| for token
  // validation and the |top_frame_origin| as the partition origin. An empty
  // optional should be passed for |top_frame_origin| if the request is a main
  // frame navigation request.
  // TODO(crbug.com/40254225): Switch |top_frame_origin| to use Cookie
  // partitioning.
  CriticalOriginTrialsThrottle(
      OriginTrialsControllerDelegate& origin_trials_delegate,
      std::optional<url::Origin> top_frame_origin,
      std::optional<ukm::SourceId> source_id);

  ~CriticalOriginTrialsThrottle() override;

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

  // This throttle only handles navigation requests. Use this method to
  // determine if the throttle will handle the passed-in |request| before
  // constructing a throttle.
  static bool IsNavigationRequest(const network::ResourceRequest& request);

 private:
  // The delegate is owned by the BrowserContext, and is expected to outlive
  // this throttle.
  raw_ref<OriginTrialsControllerDelegate, DanglingUntriaged>
      origin_trials_delegate_;

  std::optional<url::Origin> top_frame_origin_;

  bool is_navigation_request_ = false;

  // Ensure that there's only one restart per origin.
  base::flat_set<url::Origin> restarted_origins_;

  // Url of the last request made.
  GURL request_url_;

  // UKM source ID for the last request made.
  std::optional<ukm::SourceId> source_id_;

  // Trials that were persisted for the origin at the beginning of the request.
  base::flat_set<std::string> original_persisted_trials_;

  // Determine if critical origin trials have been enabled by the server
  // response and a restart is required.
  void MaybeRestartWithTrials(
      const network::mojom::URLResponseHead& response_head,
      RestartWithURLReset* restart_with_url_reset);

  // Stores the pre-request information, so it can be compared with the received
  // response headers.
  void SetPreRequestFields(const GURL& request_url);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ORIGIN_TRIALS_CRITICAL_ORIGIN_TRIALS_THROTTLE_H_
