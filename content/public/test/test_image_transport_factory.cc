// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_image_transport_factory.h"

#include <limits>
#include <utility>

#include "components/viz/test/test_in_process_context_provider.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {
namespace {

// TODO(kylechar): Use the same client id for the browser everywhere.
constexpr uint32_t kDefaultClientId = std::numeric_limits<uint32_t>::max();

}  // namespace

TestImageTransportFactory::TestImageTransportFactory()
    : frame_sink_id_allocator_(kDefaultClientId) {
  mojo::PendingRemote<viz::mojom::FrameSinkManager> frame_sink_manager;
  mojo::PendingReceiver<viz::mojom::FrameSinkManager>
      frame_sink_manager_receiver =
          frame_sink_manager.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<viz::mojom::FrameSinkManagerClient>
      frame_sink_manager_client;
  mojo::PendingReceiver<viz::mojom::FrameSinkManagerClient>
      frame_sink_manager_client_receiver =
          frame_sink_manager_client.InitWithNewPipeAndPassReceiver();

  // Bind endpoints in HostFrameSinkManager.
  host_frame_sink_manager_.BindAndSetManager(
      std::move(frame_sink_manager_client_receiver), nullptr,
      std::move(frame_sink_manager));

  // Bind endpoints in TestFrameSinkManagerImpl. For non-tests there would be
  // a FrameSinkManagerImpl running in another process and these interface
  // endpoints would be bound there.
  test_frame_sink_manager_impl_.BindReceiver(
      std::move(frame_sink_manager_receiver),
      std::move(frame_sink_manager_client));
}

TestImageTransportFactory::~TestImageTransportFactory() = default;

void TestImageTransportFactory::CreateLayerTreeFrameSink(
    base::WeakPtr<ui::Compositor> compositor) {
  compositor->SetLayerTreeFrameSink(
      cc::FakeLayerTreeFrameSink::Create3d(),
      mojo::AssociatedRemote<viz::mojom::DisplayPrivate>());
}

scoped_refptr<viz::RasterContextProvider>
TestImageTransportFactory::SharedMainThreadRasterContextProvider() {
  NOTIMPLEMENTED();
  return nullptr;
}

gpu::GpuMemoryBufferManager*
TestImageTransportFactory::GetGpuMemoryBufferManager() {
  return &gpu_memory_buffer_manager_;
}

cc::TaskGraphRunner* TestImageTransportFactory::GetTaskGraphRunner() {
  return &task_graph_runner_;
}

viz::FrameSinkId TestImageTransportFactory::AllocateFrameSinkId() {
  return frame_sink_id_allocator_.NextFrameSinkId();
}

viz::SubtreeCaptureId TestImageTransportFactory::AllocateSubtreeCaptureId() {
  return subtree_capture_id_allocator_.NextSubtreeCaptureId();
}

viz::HostFrameSinkManager*
TestImageTransportFactory::GetHostFrameSinkManager() {
  return &host_frame_sink_manager_;
}

void TestImageTransportFactory::DisableGpuCompositing() {
  NOTIMPLEMENTED();
}

ui::ContextFactory* TestImageTransportFactory::GetContextFactory() {
  return this;
}

}  // namespace content
