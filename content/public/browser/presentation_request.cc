// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/presentation_request.h"

namespace content {

PresentationRequest::PresentationRequest(
    const GlobalRenderFrameHostId& render_frame_host_id,
    const std::vector<GURL>& presentation_urls,
    const url::Origin& frame_origin)
    : render_frame_host_id(render_frame_host_id),
      presentation_urls(presentation_urls),
      frame_origin(frame_origin) {}

PresentationRequest::~PresentationRequest() = default;

PresentationRequest::PresentationRequest(const PresentationRequest& other) =
    default;

PresentationRequest& PresentationRequest::operator=(
    const PresentationRequest& other) = default;

bool PresentationRequest::operator==(const PresentationRequest& other) const {
  return render_frame_host_id == other.render_frame_host_id &&
         presentation_urls == other.presentation_urls &&
         frame_origin == other.frame_origin;
}

}  // namespace content
