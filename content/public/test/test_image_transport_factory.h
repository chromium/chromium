// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_IMAGE_TRANSPORT_FACTORY_H_
#define CONTENT_PUBLIC_TEST_TEST_IMAGE_TRANSPORT_FACTORY_H_

#include "build/build_config.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/test_task_graph_runner.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "components/viz/common/surfaces/subtree_capture_id_allocator.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/test/test_frame_sink_manager.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "gpu/command_buffer/client/test_gpu_memory_buffer_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/compositor/compositor.h"

namespace content {

// Test implementation of ImageTransportFactory and ContextFactory. This class
// tries to do very little, mostly setting up HostFrameSinkManager and returning
// fake implementations where possible.
class TestImageTransportFactory : public ui::ContextFactory,
                                  public ImageTransportFactory {
 public:
  TestImageTransportFactory();

  TestImageTransportFactory(const TestImageTransportFactory&) = delete;
  TestImageTransportFactory& operator=(const TestImageTransportFactory&) =
      delete;

  ~TestImageTransportFactory() override;

  // ui::ContextFactory implementation.
  void CreateLayerTreeFrameSink(
      base::WeakPtr<ui::Compositor> compositor) override;
  scoped_refptr<viz::RasterContextProvider>
  SharedMainThreadRasterContextProvider() override;
  void RemoveCompositor(ui::Compositor* compositor) override {}
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  viz::FrameSinkId AllocateFrameSinkId() override;
  viz::SubtreeCaptureId AllocateSubtreeCaptureId() override;
  viz::HostFrameSinkManager* GetHostFrameSinkManager() override;

  // ImageTransportFactory implementation.
  void DisableGpuCompositing() override;
  ui::ContextFactory* GetContextFactory() override;

 private:
  cc::TestTaskGraphRunner task_graph_runner_;
  gpu::TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  viz::RendererSettings renderer_settings_;
  viz::FrameSinkIdAllocator frame_sink_id_allocator_;
  viz::SubtreeCaptureIdAllocator subtree_capture_id_allocator_;
  viz::HostFrameSinkManager host_frame_sink_manager_;
  viz::TestFrameSinkManagerImpl test_frame_sink_manager_impl_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_IMAGE_TRANSPORT_FACTORY_H_
