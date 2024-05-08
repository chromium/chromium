// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDER_WIDGET_HOST_FACTORY_H_
#define CONTENT_TEST_TEST_RENDER_WIDGET_HOST_FACTORY_H_

#include <stdint.h>

#include "content/browser/renderer_host/render_widget_host_factory.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {

// Manages creation of the RenderWidgetHostImpls using our special subclass.
// This automatically registers itself when it goes in scope, and unregisters
// itself when it goes out of scope. Since you can't have more than one factory
// registered at a time, you can only have one of these objects at a time.
class TestRenderWidgetHostFactory : public RenderWidgetHostFactory {
 public:
  TestRenderWidgetHostFactory();

  TestRenderWidgetHostFactory(const TestRenderWidgetHostFactory&) = delete;
  TestRenderWidgetHostFactory& operator=(const TestRenderWidgetHostFactory&) =
      delete;

  ~TestRenderWidgetHostFactory() override;

  std::unique_ptr<RenderWidgetHostImpl> CreateRenderWidgetHost(
      FrameTree* frame_tree,
      RenderWidgetHostDelegate* delegate,
      viz::FrameSinkId frame_sink_id,
      base::SafeRef<SiteInstanceGroup> site_instance_group,
      int32_t routing_id,
      bool hidden,
      bool renderer_initiated_creation) override;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDER_WIDGET_HOST_FACTORY_H_
