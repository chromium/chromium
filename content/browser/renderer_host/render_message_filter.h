// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "content/common/render_message_filter.mojom.h"

namespace content {
class RenderWidgetHelper;

// This class processes timing critical messages on the IO thread.
class RenderMessageFilter : public mojom::RenderMessageFilter {
 public:
  // Create the filter.
  RenderMessageFilter(int render_process_id,
                      RenderWidgetHelper* render_widget_helper);
  ~RenderMessageFilter() override;

  RenderMessageFilter(const RenderMessageFilter&) = delete;
  RenderMessageFilter& operator=(const RenderMessageFilter&) = delete;

 private:
  mojom::FrameRoutingInfoPtr AllocateNewRoutingInfo();

  // mojom::RenderMessageFilter:
  void GenerateSingleFrameRoutingInfo(
      GenerateSingleFrameRoutingInfoCallback callback) override;
  void GenerateFrameRoutingInfos(
      GenerateFrameRoutingInfosCallback callback) override;

  scoped_refptr<RenderWidgetHelper> render_widget_helper_;

  const int render_process_id_;
  const int cache_response_size_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_
