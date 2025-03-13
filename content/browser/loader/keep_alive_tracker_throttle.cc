// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/keep_alive_tracker_throttle.h"

#include <string>
#include <string_view>

#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/browser/loader/keep_alive_attribution_request_helper.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace content {
namespace {

constexpr char kUrlCategoryParamName[] = "category";

bool HasCategory(const network::ResourceRequest& request) {
  std::string category;
  return net::GetValueForKeyInQuery(request.url, kUrlCategoryParamName,
                                    &category);
}

}  // namespace

// static
std::unique_ptr<KeepAliveTrackerThrottle>
KeepAliveTrackerThrottle::MaybeCreateKeepAliveTrackerThrottle(
    const network::ResourceRequest& request) {
  if (!request.keepalive) {
    return nullptr;
  }

  if (!base::FeatureList::IsEnabled(blink::features::kBeaconLeakageLogging)) {
    return nullptr;
  }

  RequestType request_type = RequestType::kFetch;
  if (KeepAliveAttributionRequestHelper::IsAttributionRequest(request)) {
    request_type = RequestType::kAttribution;
  }

  if (!HasCategory(request)) {
    return nullptr;
  }
  // TODO(crbug.com/382527001 ): Add request filtering by category.
  // https://docs.google.com/document/d/1FY3AINZW_h2GU81U-XPQz8bKT6wgt0u7Z9GF4o7_Llk/edit?resourcekey=0-lQnRY9BK0iclRRV62LgVDQ&tab=t.0#heading=h.w60avsax5vmp

  return base::WrapUnique(new KeepAliveTrackerThrottle(request_type));
}

KeepAliveTrackerThrottle::KeepAliveTrackerThrottle(RequestType request_type)
    : request_type_(request_type) {}

KeepAliveTrackerThrottle::~KeepAliveTrackerThrottle() = default;

void KeepAliveTrackerThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  // TODO(crbug.com/382527001): Add UKM logging.
}

void KeepAliveTrackerThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  // TODO(crbug.com/382527001): Add UKM logging.
}

void KeepAliveTrackerThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers,
    net::HttpRequestHeaders* modified_cors_exempt_request_headers) {
  num_redirects_++;
  // TODO(crbug.com/382527001): Add UKM logging.
}

void KeepAliveTrackerThrottle::WillOnCompleteWithError(
    const network::URLLoaderCompletionStatus& status) {
  // TODO(crbug.com/382527001): Add UKM logging.
}

}  // namespace content
