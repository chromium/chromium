// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDER_WIDGET_HOST_H_
#define CONTENT_TEST_TEST_RENDER_WIDGET_HOST_H_

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/test/mock_widget_input_handler.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {
// TestRenderWidgetHostView ----------------------------------------------------

// Subclass the RenderWidgetHostImpl so that we can watch the mojo
// input channel.
class TestRenderWidgetHost : public RenderWidgetHostImpl {
 public:
  static std::unique_ptr<RenderWidgetHostImpl> Create(
      FrameTree* frame_tree,
      RenderWidgetHostDelegate* delegate,
      viz::FrameSinkId frame_sink_id,
      base::SafeRef<SiteInstanceGroup> site_instance_group,
      int32_t routing_id,
      bool hidden,
      bool renderer_initiated_creation);

  ~TestRenderWidgetHost() override;

  // RenderWidgetHostImpl overrides.
  blink::mojom::WidgetInputHandler* GetWidgetInputHandler() override;

  MockWidgetInputHandler* GetMockWidgetInputHandler();

  static mojo::PendingAssociatedRemote<blink::mojom::Widget>
  CreateStubWidgetRemote();
  static mojo::PendingAssociatedRemote<blink::mojom::FrameWidget>
  CreateStubFrameWidgetRemote();

 private:
  TestRenderWidgetHost(FrameTree* frame_tree,
                       RenderWidgetHostDelegate* delegate,
                       viz::FrameSinkId frame_sink_id,
                       base::SafeRef<SiteInstanceGroup> site_instance_group,
                       int32_t routing_id,
                       bool hidden,
                       bool renderer_initiated_creation);
  MockWidgetInputHandler input_handler_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDER_WIDGET_HOST_H_
