// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_url_request_extra_data.h"

#include "services/network/public/cpp/resource_request.h"

namespace blink {

WebURLRequestExtraData::WebURLRequestExtraData() = default;

WebURLRequestExtraData::~WebURLRequestExtraData() = default;

void WebURLRequestExtraData::CopyToResourceRequest(
    network::ResourceRequest* request) const {
  request->is_outermost_main_frame = is_outermost_main_frame_;
  request->transition_type = transition_type_;
  request->originated_from_service_worker = originated_from_service_worker_;
}

}  // namespace blink
