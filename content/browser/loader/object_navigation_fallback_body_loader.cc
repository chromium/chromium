// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/object_navigation_fallback_body_loader.h"

#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/loader/resource_timing_utils.h"
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
  blink::mojom::ResourceTimingInfoPtr timing_info =
      GenerateResourceTimingForNavigation(
          render_frame_host->GetParent()->GetLastCommittedOrigin(),
          common_params, commit_params, response_head);
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
  response_body_drainer_.reset();
  // At this point, `this` is done and the associated NavigationRequest and
  // `this` must be cleaned up, no matter what else happens. Running
  // `completion_closure_` will delete the NavigationRequest, which will delete
  // `this`.
  base::ScopedClosureRunner cleanup(std::move(completion_closure_));

  timing_info_->response_end = status.completion_time;
  timing_info_->encoded_body_size = status.encoded_body_length;
  timing_info_->decoded_body_size = status.decoded_body_length;

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

void ObjectNavigationFallbackBodyLoader::OnDataAvailable(const void* data,
                                                         size_t num_bytes) {}

void ObjectNavigationFallbackBodyLoader::OnDataComplete() {}

}  // namespace content
