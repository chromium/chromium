// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/resource_load_stats.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "content/common/net/record_load_histograms.h"
#include "content/public/common/resource_load_info.mojom.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "net/base/ip_endpoint.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/url_loader_completion_status.h"

namespace content {

namespace {

// Returns true if the headers indicate that this resource should always be
// revalidated or not cached.
bool AlwaysAccessNetwork(
    const scoped_refptr<net::HttpResponseHeaders>& headers) {
  if (!headers)
    return false;

  // RFC 2616, section 14.9.
  return headers->HasHeaderValue("cache-control", "no-cache") ||
         headers->HasHeaderValue("cache-control", "no-store") ||
         headers->HasHeaderValue("pragma", "no-cache") ||
         headers->HasHeaderValue("vary", "*");
}

#if defined(OS_ANDROID)
void UpdateUserGestureCarryoverInfo(int render_frame_id) {
  RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(render_frame_id);
  if (frame)
    frame->GetFrameHost()->UpdateUserGestureCarryoverInfo();
}
#endif

void ResourceResponseReceived(int render_frame_id,
                              int request_id,
                              const GURL& final_response_url,
                              network::mojom::URLResponseHeadPtr response_head,
                              content::ResourceType resource_type,
                              PreviewsState previews_state) {
  RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(render_frame_id);
  if (!frame)
    return;
  url::Origin origin_of_final_response_url =
      url::Origin::Create(final_response_url);
  if (!IsResourceTypeFrame(resource_type)) {
    frame->GetFrameHost()->SubresourceResponseStarted(
        origin_of_final_response_url, response_head->cert_status);
  }
  frame->DidStartResponse(origin_of_final_response_url, request_id,
                          std::move(response_head), resource_type,
                          previews_state);
}

void ResourceTransferSizeUpdated(int render_frame_id,
                                 int request_id,
                                 int transfer_size_diff) {
  RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(render_frame_id);
  if (frame)
    frame->DidReceiveTransferSizeUpdate(request_id, transfer_size_diff);
}

void ResourceLoadCompleted(int render_frame_id,
                           mojom::ResourceLoadInfoPtr resource_load_info,
                           const network::URLLoaderCompletionStatus& status) {
  RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(render_frame_id);
  if (!frame)
    return;
  frame->DidCompleteResponse(resource_load_info->request_id, status);
  frame->GetFrameHost()->ResourceLoadComplete(std::move(resource_load_info));
}

void ResourceLoadCanceled(int render_frame_id, int request_id) {
  RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(render_frame_id);
  if (frame)
    frame->DidCancelResponse(request_id);
}

}  // namespace

#if defined(OS_ANDROID)
void NotifyUpdateUserGestureCarryoverInfo(int render_frame_id) {
  auto task_runner = RenderThreadImpl::DeprecatedGetMainTaskRunner();
  if (!task_runner)
    return;
  if (task_runner->BelongsToCurrentThread()) {
    UpdateUserGestureCarryoverInfo(render_frame_id);
    return;
  }
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(UpdateUserGestureCarryoverInfo, render_frame_id));
}
#endif

mojom::ResourceLoadInfoPtr NotifyResourceLoadInitiated(
    int render_frame_id,
    int request_id,
    const GURL& request_url,
    const std::string& http_method,
    const GURL& referrer,
    ResourceType resource_type,
    net::RequestPriority request_priority) {
  auto resource_load_info = mojom::ResourceLoadInfo::New();
  resource_load_info->method = http_method;
  resource_load_info->original_url = request_url;
  resource_load_info->url = request_url;
  resource_load_info->resource_type = resource_type;
  resource_load_info->request_id = request_id;
  resource_load_info->referrer = referrer;
  resource_load_info->network_info = mojom::CommonNetworkInfo::New();
  resource_load_info->request_priority = request_priority;
  return resource_load_info;
}

void NotifyResourceRedirectReceived(
    int render_frame_id,
    mojom::ResourceLoadInfo* resource_load_info,
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr redirect_response) {
  resource_load_info->url = redirect_info.new_url;
  resource_load_info->method = redirect_info.new_method;
  resource_load_info->referrer = GURL(redirect_info.new_referrer);
  mojom::RedirectInfoPtr net_redirect_info = mojom::RedirectInfo::New();
  net_redirect_info->url = redirect_info.new_url;
  net_redirect_info->network_info = mojom::CommonNetworkInfo::New();
  net_redirect_info->network_info->network_accessed =
      redirect_response->network_accessed;
  net_redirect_info->network_info->always_access_network =
      AlwaysAccessNetwork(redirect_response->headers);
  net_redirect_info->network_info->remote_endpoint =
      redirect_response->remote_endpoint;
  resource_load_info->redirect_info_chain.push_back(
      std::move(net_redirect_info));
}

void NotifyResourceResponseReceived(
    int render_frame_id,
    mojom::ResourceLoadInfo* resource_load_info,
    network::mojom::URLResponseHeadPtr response_head,
    PreviewsState previews_state) {
  if (response_head->network_accessed) {
    if (resource_load_info->resource_type == ResourceType::kMainFrame) {
      UMA_HISTOGRAM_ENUMERATION("Net.ConnectionInfo.MainFrame",
                                response_head->connection_info,
                                net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS);
    } else {
      UMA_HISTOGRAM_ENUMERATION("Net.ConnectionInfo.SubResource",
                                response_head->connection_info,
                                net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS);
    }
  }

  resource_load_info->mime_type = response_head->mime_type;
  resource_load_info->load_timing_info = response_head->load_timing;
  resource_load_info->network_info->network_accessed =
      response_head->network_accessed;
  resource_load_info->network_info->always_access_network =
      AlwaysAccessNetwork(response_head->headers);
  resource_load_info->network_info->remote_endpoint =
      response_head->remote_endpoint;

  auto task_runner = RenderThreadImpl::DeprecatedGetMainTaskRunner();
  if (!task_runner)
    return;
  if (task_runner->BelongsToCurrentThread()) {
    ResourceResponseReceived(render_frame_id, resource_load_info->request_id,
                             resource_load_info->url, std::move(response_head),
                             resource_load_info->resource_type, previews_state);
    return;
  }

  // Make a deep copy of URLResponseHead before passing it cross-thread.
  if (response_head->headers) {
    response_head->headers =
        new net::HttpResponseHeaders(response_head->headers->raw_headers());
  }
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(ResourceResponseReceived, render_frame_id,
                     resource_load_info->request_id, resource_load_info->url,
                     std::move(response_head),
                     resource_load_info->resource_type, previews_state));
}

void NotifyResourceTransferSizeUpdated(
    int render_frame_id,
    mojom::ResourceLoadInfo* resource_load_info,
    int transfer_size_diff) {
  auto task_runner = RenderThreadImpl::DeprecatedGetMainTaskRunner();
  if (!task_runner)
    return;
  if (task_runner->BelongsToCurrentThread()) {
    ResourceTransferSizeUpdated(render_frame_id, resource_load_info->request_id,
                                transfer_size_diff);
    return;
  }
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(ResourceTransferSizeUpdated, render_frame_id,
                     resource_load_info->request_id, transfer_size_diff));
}

void NotifyResourceLoadCompleted(
    int render_frame_id,
    mojom::ResourceLoadInfoPtr resource_load_info,
    const network::URLLoaderCompletionStatus& status) {
  RecordLoadHistograms(resource_load_info->url,
                       resource_load_info->resource_type, status.error_code);

  resource_load_info->was_cached = status.exists_in_cache;
  resource_load_info->net_error = status.error_code;
  resource_load_info->total_received_bytes = status.encoded_data_length;
  resource_load_info->raw_body_bytes = status.encoded_body_length;

  auto task_runner = RenderThreadImpl::DeprecatedGetMainTaskRunner();
  if (!task_runner)
    return;
  if (task_runner->BelongsToCurrentThread()) {
    ResourceLoadCompleted(render_frame_id, std::move(resource_load_info),
                          status);
    return;
  }
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(ResourceLoadCompleted, render_frame_id,
                                       std::move(resource_load_info), status));
}

void NotifyResourceLoadCanceled(int render_frame_id,
                                mojom::ResourceLoadInfoPtr resource_load_info,
                                int net_error) {
  RecordLoadHistograms(resource_load_info->url,
                       resource_load_info->resource_type, net_error);

  auto task_runner = RenderThreadImpl::DeprecatedGetMainTaskRunner();
  if (!task_runner)
    return;
  if (task_runner->BelongsToCurrentThread()) {
    ResourceLoadCanceled(render_frame_id, resource_load_info->request_id);
    return;
  }
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(ResourceLoadCanceled, render_frame_id,
                                       resource_load_info->request_id));
}

}  // namespace content
