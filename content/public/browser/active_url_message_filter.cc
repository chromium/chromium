// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/active_url_message_filter.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace internal {

ActiveUrlMessageFilter::ActiveUrlMessageFilter(
    RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {}

ActiveUrlMessageFilter::~ActiveUrlMessageFilter() {
  if (debug_url_set_) {
    GetContentClient()->SetActiveURL(GURL(), "");
  }
}

bool ActiveUrlMessageFilter::WillDispatch(mojo::Message* message) {
  debug_url_set_ = true;
  GetContentClient()->SetActiveURL(render_frame_host_->GetLastCommittedURL(),
                                   render_frame_host_->GetOutermostMainFrame()
                                       ->GetLastCommittedOrigin()
                                       .GetDebugString());
  return true;
}

void ActiveUrlMessageFilter::DidDispatchOrReject(mojo::Message* message,
                                                 bool accepted) {
  GetContentClient()->SetActiveURL(GURL(), "");
  debug_url_set_ = false;
}

}  // namespace internal
}  // namespace content
