// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_url_request_extra_data.h"

#include "services/network/public/cpp/resource_request.h"

namespace blink {

WebURLRequestExtraData::WebURLRequestExtraData()
    : render_frame_id_(MSG_ROUTING_NONE) {}

WebURLRequestExtraData::~WebURLRequestExtraData() = default;

void WebURLRequestExtraData::CopyToResourceRequest(
    network::ResourceRequest* request) const {
  request->render_frame_id = render_frame_id_.value();
  request->is_main_frame = is_main_frame_;
  request->transition_type = transition_type_;
  request->originated_from_service_worker = originated_from_service_worker_;
  request->force_ignore_site_for_cookies = force_ignore_site_for_cookies_;
}

}  // namespace blink
