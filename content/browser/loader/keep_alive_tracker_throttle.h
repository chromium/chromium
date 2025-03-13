// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_KEEP_ALIVE_TRACKER_THROTTLE_H_
#define CONTENT_BROWSER_LOADER_KEEP_ALIVE_TRACKER_THROTTLE_H_

#include "content/common/content_export.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/gurl.h"

namespace content {

// KeepAliveTrackerThrottle is responsible for recording browser-side metrics
// for an eligible fetch keepalive request.
//
// See
// https://docs.google.com/document/d/1byKFqqKTsVFnj6rSb7ZjHi4LuRi1LNb-vf4maQbmSSQ/edit?resourcekey=0-7GAe1ae8j_55DMcvp3U3IQ&tab=t.0#heading=h.bk553qrz82t0
class CONTENT_EXPORT KeepAliveTrackerThrottle
    : public blink::URLLoaderThrottle {
 public:
  // The type of request to track for in this throttle. Anything beyond the
  // ones listed here will not be recorded.
  enum class RequestType {
    kFetch = 0,
    kAttribution = 1,
  };

  ~KeepAliveTrackerThrottle() override;
  // Not movable.
  KeepAliveTrackerThrottle(const KeepAliveTrackerThrottle&) = delete;
  KeepAliveTrackerThrottle& operator=(const KeepAliveTrackerThrottle&) = delete;

  // Returns a `KeepAliveTrackerThrottle` instance if `request` is eligible to
  // be tracked by this throttle.
  static std::unique_ptr<KeepAliveTrackerThrottle>
  MaybeCreateKeepAliveTrackerThrottle(const network::ResourceRequest& request);

  // `blink::URLLoaderThrottle` overrides:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_request_headers,
      net::HttpRequestHeaders* modified_request_headers,
      net::HttpRequestHeaders* modified_cors_exempt_request_headers) override;
  void WillOnCompleteWithError(
      const network::URLLoaderCompletionStatus& status) override;

  // Testing only:
  // TODO(crbug.com/382527001): Remove after UKM logging is added.
  RequestType GetRequestTypeForTesting() { return request_type_; }

 private:
  explicit KeepAliveTrackerThrottle(RequestType request_type);

  // The type of the fetch keepalive request this throttle is running for.
  const RequestType request_type_;

  // Records the number of redrects the tracked fetch keepalive request has
  // experienced so far.
  uint32_t num_redirects_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_KEEP_ALIVE_TRACKER_THROTTLE_H_
