// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_timing_utils.h"

#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "net/http/http_response_info.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace {

// Implements the TimingAllowOrigin check
// This logic is duplicated from Performance::AllowsTimingRedirect(). Ensure
// that any changes are synced between both copies.
bool IsCrossOriginResponseOrHasCrossOriginRedirects(
    const url::Origin& parent_origin,
    const blink::mojom::CommonNavigationParams& common_params,
    const blink::mojom::CommitNavigationParams& commit_params) {
  DCHECK_EQ(commit_params.redirect_infos.size(),
            commit_params.redirect_response.size());
  for (const auto& info : commit_params.redirect_infos) {
    if (!parent_origin.IsSameOriginWith(info.new_url)) {
      return true;
    }
  }

  return !parent_origin.IsSameOriginWith(common_params.url);
}

}  // namespace

namespace content {

// This logic is duplicated from blink::CreateResourceTimingInfo(). Ensure
// that any changes are synced between both copies.
blink::mojom::ResourceTimingInfoPtr GenerateResourceTimingForNavigation(
    const url::Origin& parent_origin,
    const blink::mojom::CommonNavigationParams& common_params,
    const blink::mojom::CommitNavigationParams& commit_params,
    const network::mojom::URLResponseHead& response_head) {
  // TODO(dcheng): There should be a Blink helper for populating the timing info
  // that's exposed in //third_party/blink/common. This would allow a lot of the
  // boilerplate to be shared.

  auto timing_info = blink::mojom::ResourceTimingInfo::New();
  const GURL& initial_url = !commit_params.original_url.is_empty()
                                ? commit_params.original_url
                                : common_params.url;
  timing_info->name = initial_url.spec();
  timing_info->start_time = common_params.navigation_start;
  timing_info->allow_timing_details = response_head.timing_allow_passed;

  // Only expose the response code when we are same origin and without
  // cross-origin redirects
  // https://fetch.spec.whatwg.org/#ref-for-concept-response-status%E2%91%A6
  if (!IsCrossOriginResponseOrHasCrossOriginRedirects(
          parent_origin, common_params, commit_params)) {
    timing_info->response_status = commit_params.http_response_code;
  }

  // https://fetch.spec.whatwg.org/#create-an-opaque-timing-info
  if (!timing_info->allow_timing_details) {
    return timing_info;
  }

  timing_info->alpn_negotiated_protocol =
      response_head.alpn_negotiated_protocol;
  timing_info->connection_info = net::HttpResponseInfo::ConnectionInfoToString(
      response_head.connection_info);

  // If there's no received headers end time, don't set load timing. This is the
  // case for non-HTTP requests, requests that don't go over the wire, and
  // certain error cases.
  // TODO(dcheng): Is it actually possible to hit this path if
  // `response_head.headers` is populated?
  if (!response_head.load_timing.receive_headers_end.is_null()) {
    timing_info->timing = response_head.load_timing;
  }
  // `response_end` will be populated after loading the body.

  DCHECK_EQ(commit_params.redirect_infos.size(),
            commit_params.redirect_response.size());

  if (!commit_params.redirect_infos.empty()) {
    timing_info->last_redirect_end_time =
        commit_params.redirect_response.back()->load_timing.receive_headers_end;
  } else {
    timing_info->last_redirect_end_time = base::TimeTicks();
  }
  // The final value for `encoded_body_size` and `decoded_body_size` will be
  // populated after loading the body.
  timing_info->did_reuse_connection = response_head.load_timing.socket_reused;
  // Use url::Origin to handle cases like blob:https://.
  timing_info->is_secure_transport = base::Contains(
      url::GetSecureSchemes(), url::Origin::Create(common_params.url).scheme());
  timing_info->allow_negative_values = false;
  return timing_info;
}
}  // namespace content
