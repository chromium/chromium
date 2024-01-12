// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_message_filter.h"

#include <errno.h>

#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {

RenderMessageFilter::RenderMessageFilter(
    int render_process_id,
    RenderWidgetHelper* render_widget_helper)
    : render_widget_helper_(render_widget_helper),
      render_process_id_(render_process_id) {
  if (render_widget_helper)
    render_widget_helper_->Init(render_process_id_);
}

RenderMessageFilter::~RenderMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void RenderMessageFilter::GenerateFrameRoutingID(
    GenerateFrameRoutingIDCallback callback) {
  int32_t routing_id = render_widget_helper_->GetNextRoutingID();
  auto frame_token = blink::LocalFrameToken();
  auto devtools_frame_token = base::UnguessableToken::Create();
  auto document_token = blink::DocumentToken();
  render_widget_helper_->StoreNextFrameRoutingID(
      routing_id, frame_token, devtools_frame_token, document_token);
  std::move(callback).Run(routing_id, frame_token, devtools_frame_token,
                          document_token);
}

}  // namespace content
