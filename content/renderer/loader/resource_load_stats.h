// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_RESOURCE_LOAD_STATS_H_
#define CONTENT_RENDERER_LOADER_RESOURCE_LOAD_STATS_H_

#include "build/build_config.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_load_info.mojom.h"
#include "content/public/common/resource_type.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

class GURL;

namespace net {
struct RedirectInfo;
}  // namespace net

namespace network {
struct URLLoaderCompletionStatus;
}  // namespace network

namespace content {

// A collection of methods collecting histograms related to resource load
// and notifying browser process with loading stats.
//
// Each resource load should start with NotifyResourceLoadInitiated,
// and then pass returned mojom::ResourceLoadInfo to all subsequent
// calls until NotifyResourceLoadCompleted or NotifyResourceLoadCanceled.

#if defined(OS_ANDROID)
void NotifyUpdateUserGestureCarryoverInfo(int render_frame_id);
#endif

mojom::ResourceLoadInfoPtr NotifyResourceLoadInitiated(
    int render_frame_id,
    int request_id,
    const GURL& request_url,
    const std::string& http_method,
    const GURL& referrer,
    ResourceType resource_type,
    net::RequestPriority request_priority);

void NotifyResourceRedirectReceived(
    int render_frame_id,
    mojom::ResourceLoadInfo* resource_load_info,
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr redirect_response);

void NotifyResourceResponseReceived(
    int render_frame_id,
    mojom::ResourceLoadInfo* resource_load_info,
    network::mojom::URLResponseHeadPtr response_head,
    PreviewsState previews_state);

void NotifyResourceTransferSizeUpdated(
    int render_frame_id,
    mojom::ResourceLoadInfo* resource_load_info,
    int transfer_size_diff);

void NotifyResourceLoadCompleted(
    int render_frame_id,
    mojom::ResourceLoadInfoPtr resource_load_info,
    const network::URLLoaderCompletionStatus& status);

void NotifyResourceLoadCanceled(int render_frame_id,
                                mojom::ResourceLoadInfoPtr resource_load_info,
                                int net_error);

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_RESOURCE_LOAD_STATS_H_
