// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/request_extra_data.h"

#include "content/common/service_worker/service_worker_types.h"
#include "services/network/public/cpp/resource_request.h"

using blink::WebString;

namespace content {

RequestExtraData::RequestExtraData()
    : visibility_state_(blink::mojom::PageVisibilityState::kVisible),
      render_frame_id_(MSG_ROUTING_NONE),
      is_main_frame_(false),
      allow_download_(true),
      transition_type_(ui::PAGE_TRANSITION_LINK),
      service_worker_provider_id_(kInvalidServiceWorkerProviderId),
      originated_from_service_worker_(false),
      initiated_in_secure_context_(false),
      is_for_no_state_prefetch_(false),
      block_mixed_plugin_content_(false),
      navigation_initiated_by_renderer_(false),
      attach_same_site_cookies_(false) {}

RequestExtraData::~RequestExtraData() {
}

void RequestExtraData::CopyToResourceRequest(
    network::ResourceRequest* request) const {
  request->is_prerendering =
      visibility_state_ == blink::mojom::PageVisibilityState::kPrerender;
  request->render_frame_id = render_frame_id_;
  request->is_main_frame = is_main_frame_;

  request->allow_download = allow_download_;
  request->transition_type = transition_type_;
  request->service_worker_provider_id = service_worker_provider_id_;
  request->originated_from_service_worker = originated_from_service_worker_;

  request->initiated_in_secure_context = initiated_in_secure_context_;
  request->attach_same_site_cookies = attach_same_site_cookies_;
}

}  // namespace content
