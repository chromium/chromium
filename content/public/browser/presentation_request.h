// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRESENTATION_REQUEST_H_
#define CONTENT_PUBLIC_BROWSER_PRESENTATION_REQUEST_H_

#include <utility>
#include <vector>

#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// Represents a presentation request made from a render frame. Contains a list
// of presentation URLs of the request, and information on the originating
// frame.
struct CONTENT_EXPORT PresentationRequest {
 public:
  PresentationRequest(const GlobalRenderFrameHostId& render_frame_host_id,
                      const std::vector<GURL>& presentation_urls,
                      const url::Origin& frame_origin);
  ~PresentationRequest();

  PresentationRequest(const PresentationRequest& other);
  PresentationRequest& operator=(const PresentationRequest& other);

  bool operator==(const PresentationRequest& other) const;
  bool operator!=(const PresentationRequest& other) const {
    return !(*this == other);
  }

  // ID of RenderFrameHost that initiated the request.
  GlobalRenderFrameHostId render_frame_host_id;

  // URLs of presentation.
  std::vector<GURL> presentation_urls;

  // Origin of frame from which the request was initiated.
  url::Origin frame_origin;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRESENTATION_REQUEST_H_
