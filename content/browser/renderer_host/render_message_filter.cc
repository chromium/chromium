// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_message_filter.h"

#include <errno.h>

#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {

RenderMessageFilter::RenderMessageFilter(
    int render_process_id,
    RenderWidgetHelper* render_widget_helper)
    : render_widget_helper_(render_widget_helper),
      render_process_id_(render_process_id),
      cache_response_size_(features::kFrameRoutingCacheResponseSize.Get()) {
  if (render_widget_helper) {
    render_widget_helper_->Init(render_process_id_);
  }
}

RenderMessageFilter::~RenderMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

mojom::FrameRoutingInfoPtr RenderMessageFilter::AllocateNewRoutingInfo() {
  auto result = mojom::FrameRoutingInfo::New();
  result->routing_id = render_widget_helper_->GetNextRoutingID();
  result->devtools_frame_token = base::UnguessableToken::Create();
  render_widget_helper_->StoreNextFrameRoutingID(
      result->routing_id, result->frame_token, result->devtools_frame_token,
      result->document_token);
  return result;
}

void RenderMessageFilter::GenerateSingleFrameRoutingInfo(
    GenerateSingleFrameRoutingInfoCallback callback) {
  std::move(callback).Run(AllocateNewRoutingInfo());
}

void RenderMessageFilter::GenerateFrameRoutingInfos(
    GenerateFrameRoutingInfosCallback callback) {
  std::vector<mojom::FrameRoutingInfoPtr> result;
  for (int i = 0; i < cache_response_size_; ++i) {
    result.push_back(AllocateNewRoutingInfo());
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace content
