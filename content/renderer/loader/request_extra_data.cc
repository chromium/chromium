// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/request_extra_data.h"

#include "services/network/public/cpp/resource_request.h"

using blink::WebString;

namespace content {

RequestExtraData::RequestExtraData() = default;
RequestExtraData::~RequestExtraData() = default;

void RequestExtraData::CopyToResourceRequest(
    network::ResourceRequest* request) const {
  request->render_frame_id = render_frame_id_;
  request->is_main_frame = is_main_frame_;

  request->transition_type = transition_type_;
  request->originated_from_service_worker = originated_from_service_worker_;

  request->attach_same_site_cookies = attach_same_site_cookies_;
}

}  // namespace content
