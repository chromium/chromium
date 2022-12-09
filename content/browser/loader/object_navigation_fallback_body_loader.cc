// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/object_navigation_fallback_body_loader.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_host_manager.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/http/http_response_info.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/timing_allow_origin.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace content {

namespace {

// This logic is duplicated from Performance::PassesTimingAllowCheck. Ensure
// that any changes are synced between both copies.
bool PassesTimingAllowCheck(
    const network::mojom::URLResponseHead& response_head,
    const GURL& url,
    const GURL& next_url,
    const url::Origin& parent_origin,
    bool& response_tainting_not_basic,
    bool& tainted_origin_flag) {
  const url::Origin response_origin = url::Origin::Create(url);
  const bool is_same_origin = response_origin.IsSameOriginWith(parent_origin);
  // Still same-origin and resource tainting is "basic": just return true.
  if (!response_tainting_not_basic && is_same_origin) {
    return true;
  }

  // Otherwise, a cross-origin response is currently (or has previously) been
  // handled, so resource tainting is no longer "basic".
  response_tainting_not_basic = true;

  const network::mojom::TimingAllowOriginPtr& tao =
      response_head.parsed_headers->timing_allow_origin;
  if (!tao) {
    return false;
  }

  if (tao->which() == network::mojom::TimingAllowOrigin::Tag::kAll)
    return true;

  // TODO(https://crbug.com/1128402): For now, this bookkeeping only exists to
  // stay in sync with the Blink code.
  bool is_next_resource_same_origin = true;
  if (url != next_url) {
    is_next_resource_same_origin =
        response_origin.IsSameOriginWith(url::Origin::Create(next_url));
  }

  if (!is_same_origin && !is_next_resource_same_origin) {
    tainted_origin_flag = true;
  }

  return base::Contains(tao->get_serialized_origins(),
                        parent_origin.Serialize());
}

// This logic is duplicated from Performance::AllowsTimingRedirect(). Ensure
// that any changes are synced between both copies.
//
// TODO(https://crbug.com/1201767): There is a *third* implementation of the TAO
// check in CorsURLLoader, but it exactly implements the TAO check as defined in
// the Fetch standard. Unfortunately, the definition in the standard always
// allows timing details for navigations: the response tainting is always
// considered "basic" for navigations, which means that timing details will
// always be allowed, even for cross-origin frames. Oops.
bool AllowTimingDetailsForParent(
    const url::Origin& parent_origin,
    const blink::mojom::CommonNavigationParams& common_params,
    const blink::mojom::CommitNavigationParams& commit_params,
    const network::mojom::URLResponseHead& response_head) {
  bool response_tainting_not_basic = false;
  bool tainted_origin_flag = false;

  DCHECK_EQ(commit_params.redirect_infos.size(),
            commit_params.redirect_response.size());
  for (size_t i = 0; i < commit_params.redirect_infos.size(); ++i) {
    const GURL& next_response_url =
        i + 1 < commit_params.redirect_infos.size()
            ? commit_params.redirect_infos[i + 1].new_url
            : common_params.url;
    if (!PassesTimingAllowCheck(
            *commit_params.redirect_response[i],
            commit_params.redirect_infos[i].new_url, next_response_url,
            parent_origin, response_tainting_not_basic, tainted_origin_flag)) {
      return false;
    }
  }

  return PassesTimingAllowCheck(
      response_head, common_params.url, common_params.url, parent_origin,
      response_tainting_not_basic, tainted_origin_flag);
}

// This logic is duplicated from Performance::GenerateResourceTiming(). Ensure
// that any changes are synced between both copies.
blink::mojom::ResourceTimingInfoPtr GenerateResourceTiming(
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
  timing_info->context_type = blink::mojom::RequestContextType::OBJECT;

  timing_info->allow_timing_details = AllowTimingDetailsForParent(
      parent_origin, common_params, commit_params, response_head);

  DCHECK_EQ(commit_params.redirect_infos.size(),
            commit_params.redirect_response.size());

  if (!commit_params.redirect_infos.empty()) {
    timing_info->allow_redirect_details = timing_info->allow_timing_details;
    timing_info->last_redirect_end_time =
        commit_params.redirect_response.back()->load_timing.receive_headers_end;
  } else {
    timing_info->allow_redirect_details = false;
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

std::string ExtractServerTimingValueIfNeeded(
    const network::mojom::URLResponseHead& response_head) {
  std::string value;
  if (!response_head.timing_allow_passed)
    return value;

  // Note: the renderer will be responsible for parsing the actual server
  // timing values.
  response_head.headers->GetNormalizedHeader("Server-Timing", &value);
  return value;
}

}  // namespace

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(ObjectNavigationFallbackBodyLoader);

// static
void ObjectNavigationFallbackBodyLoader::CreateAndStart(
    NavigationRequest& navigation_request,
    const blink::mojom::CommonNavigationParams& common_params,
    const blink::mojom::CommitNavigationParams& commit_params,
    const network::mojom::URLResponseHead& response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    base::OnceClosure completion_closure) {
  // This should only be called for HTTP errors.
  DCHECK(response_head.headers);
  RenderFrameHostImpl* render_frame_host =
      navigation_request.frame_tree_node()->current_frame_host();
  // A frame owned by <object> should always have a parent.
  DCHECK(render_frame_host->GetParent());
  // It's safe to snapshot the parent origin in the calculation here; if the
  // parent frame navigates, `render_frame_host_` will be deleted, which
  // triggers deletion of `this`, cancelling all remaining work.
  blink::mojom::ResourceTimingInfoPtr timing_info = GenerateResourceTiming(
      render_frame_host->GetParent()->GetLastCommittedOrigin(), common_params,
      commit_params, response_head);
  std::string server_timing_value =
      ExtractServerTimingValueIfNeeded(response_head);

  CreateForNavigationHandle(
      navigation_request, std::move(timing_info),
      std::move(server_timing_value), std::move(response_body),
      std::move(url_loader_client_endpoints), std::move(completion_closure));
}

ObjectNavigationFallbackBodyLoader::~ObjectNavigationFallbackBodyLoader() {}

ObjectNavigationFallbackBodyLoader::ObjectNavigationFallbackBodyLoader(
    NavigationHandle& navigation_handle,
    blink::mojom::ResourceTimingInfoPtr timing_info,
    std::string server_timing_value,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    base::OnceClosure completion_closure)
    : navigation_request_(static_cast<NavigationRequest&>(navigation_handle)),
      url_loader_(std::move(url_loader_client_endpoints->url_loader)),
      url_loader_client_receiver_(
          this,
          std::move(url_loader_client_endpoints->url_loader_client)),
      response_body_drainer_(
          std::make_unique<mojo::DataPipeDrainer>(this,
                                                  std::move(response_body))),
      timing_info_(std::move(timing_info)),
      server_timing_value_(std::move(server_timing_value)),
      completion_closure_(std::move(completion_closure)) {
  // Unretained is safe; `url_loader_` is owned by `this` and will not dispatch
  // callbacks after it is destroyed.
  url_loader_client_receiver_.set_disconnect_handler(
      base::BindOnce(&ObjectNavigationFallbackBodyLoader::BodyLoadFailed,
                     base::Unretained(this)));
}

void ObjectNavigationFallbackBodyLoader::MaybeComplete() {
  // Completion requires receiving the completion status from the `URLLoader`,
  // as well as the response body being completely drained.
  if (!status_ || response_body_drainer_)
    return;

  // At this point, `this` is done and the associated NavigationRequest and
  // `this` must be cleaned up, no matter what else happens. Running
  // `completion_closure_` will delete the NavigationRequest, which will delete
  // `this`.
  base::ScopedClosureRunner cleanup(std::move(completion_closure_));

  timing_info_->response_end = status_->completion_time;
  timing_info_->encoded_body_size = status_->encoded_body_length;
  timing_info_->decoded_body_size = status_->decoded_body_length;

  RenderFrameHostManager* render_manager =
      navigation_request_->frame_tree_node()->render_manager();
  if (RenderFrameProxyHost* proxy = render_manager->GetProxyToParent()) {
    if (proxy->is_render_frame_proxy_live()) {
      proxy->GetAssociatedRemoteFrame()
          ->RenderFallbackContentWithResourceTiming(std::move(timing_info_),
                                                    server_timing_value_);
    }
  } else {
    render_manager->current_frame_host()
        ->GetAssociatedLocalFrame()
        ->RenderFallbackContentWithResourceTiming(std::move(timing_info_),
                                                  server_timing_value_);
  }
}

void ObjectNavigationFallbackBodyLoader::BodyLoadFailed() {
  // At this point, `this` is done and the associated NavigationRequest and
  // `this` must be cleaned up, no matter what else happens. Running
  // `completion_closure_` will delete the NavigationRequest, which will delete
  // `this`.
  base::ScopedClosureRunner cleanup(std::move(completion_closure_));

  // The endpoint for the URL loader client was closed before the body load
  // completed. This is considered failure, so trigger the fallback content, but
  // without any timing info, since it can't be calculated.
  navigation_request_->RenderFallbackContentForObjectTag();
}

void ObjectNavigationFallbackBodyLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr) {
  // Should have already happened.
  NOTREACHED();
}

void ObjectNavigationFallbackBodyLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr,
    mojo::ScopedDataPipeConsumerHandle body,
    absl::optional<mojo_base::BigBuffer> cached_metadata) {
  // Should have already happened.
  NOTREACHED();
}

void ObjectNavigationFallbackBodyLoader::OnReceiveRedirect(
    const net::RedirectInfo&,
    network::mojom::URLResponseHeadPtr) {
  // Should have already happened.
  NOTREACHED();
}

void ObjectNavigationFallbackBodyLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback) {
  // Should have already happened.
  NOTREACHED();
}

void ObjectNavigationFallbackBodyLoader::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  // Not needed so implementation omitted.
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kObjectNavigationFallbackBodyLoader);
}

void ObjectNavigationFallbackBodyLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  status_ = status;
  MaybeComplete();
}

void ObjectNavigationFallbackBodyLoader::OnDataAvailable(const void* data,
                                                         size_t num_bytes) {}

void ObjectNavigationFallbackBodyLoader::OnDataComplete() {
  response_body_drainer_.reset();
  MaybeComplete();
}

}  // namespace content
