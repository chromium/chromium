// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDER_WIDGET_HOST_H_
#define CONTENT_TEST_TEST_RENDER_WIDGET_HOST_H_

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/test/mock_widget_impl.h"
#include "content/test/mock_widget_input_handler.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {
// TestRenderWidgetHostView ----------------------------------------------------

// Subclass the RenderWidgetHostImpl so that we can watch the mojo
// input channel.
class TestRenderWidgetHost : public RenderWidgetHostImpl {
 public:
  static std::unique_ptr<RenderWidgetHostImpl> Create(
      RenderWidgetHostDelegate* delegate,
      RenderProcessHost* process,
      int32_t routing_id,
      bool hidden);
  ~TestRenderWidgetHost() override;

  // RenderWidgetHostImpl overrides.
  mojom::WidgetInputHandler* GetWidgetInputHandler() override;

  MockWidgetInputHandler* GetMockWidgetInputHandler();

 private:
  TestRenderWidgetHost(RenderWidgetHostDelegate* delegate,
                       RenderProcessHost* process,
                       int32_t routing_id,
                       std::unique_ptr<MockWidgetImpl> widget_impl,
                       mojo::PendingRemote<mojom::Widget> widget,
                       bool hidden);

  std::unique_ptr<MockWidgetImpl> widget_impl_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDER_WIDGET_HOST_H_
