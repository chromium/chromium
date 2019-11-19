// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/test/test_image_transport_factory.h"

#include <limits>
#include <utility>

#include "components/viz/common/features.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/test/test_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/compositor/reflector.h"
#include "ui/compositor/test/in_process_context_provider.h"

namespace content {
namespace {

// TODO(kylechar): Use the same client id for the browser everywhere.
constexpr uint32_t kDefaultClientId = std::numeric_limits<uint32_t>::max();

class FakeReflector : public ui::Reflector {
 public:
  FakeReflector() = default;
  ~FakeReflector() override = default;

  void OnMirroringCompositorResized() override {}
  void AddMirroringLayer(ui::Layer* layer) override {}
  void RemoveMirroringLayer(ui::Layer* layer) override {}
};

}  // namespace

TestImageTransportFactory::TestImageTransportFactory()
    : enable_viz_(features::IsVizDisplayCompositorEnabled()),
      frame_sink_id_allocator_(kDefaultClientId) {
  if (enable_viz_) {
    test_frame_sink_manager_impl_ =
        std::make_unique<viz::TestFrameSinkManagerImpl>();

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
    test_frame_sink_manager_impl_->BindReceiver(
        std::move(frame_sink_manager_receiver),
        std::move(frame_sink_manager_client));
  } else {
    shared_bitmap_manager_ = std::make_unique<viz::ServerSharedBitmapManager>();
    frame_sink_manager_impl_ = std::make_unique<viz::FrameSinkManagerImpl>(
        shared_bitmap_manager_.get());
    surface_utils::ConnectWithLocalFrameSinkManager(
        &host_frame_sink_manager_, frame_sink_manager_impl_.get());
  }
}

TestImageTransportFactory::~TestImageTransportFactory() {
  for (auto& observer : observer_list_)
    observer.OnLostSharedContext();
}

void TestImageTransportFactory::CreateLayerTreeFrameSink(
    base::WeakPtr<ui::Compositor> compositor) {
  compositor->SetLayerTreeFrameSink(cc::FakeLayerTreeFrameSink::Create3d());
}

scoped_refptr<viz::ContextProvider>
TestImageTransportFactory::SharedMainThreadContextProvider() {
  if (shared_main_context_provider_ &&
      shared_main_context_provider_->ContextGL()->GetGraphicsResetStatusKHR() ==
          GL_NO_ERROR)
    return shared_main_context_provider_;

  constexpr bool kSupportsLocking = false;
  shared_main_context_provider_ = ui::InProcessContextProvider::CreateOffscreen(
      &gpu_memory_buffer_manager_, &image_factory_, kSupportsLocking);
  auto result = shared_main_context_provider_->BindToCurrentThread();
  if (result != gpu::ContextResult::kSuccess)
    shared_main_context_provider_ = nullptr;

  return shared_main_context_provider_;
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

void TestImageTransportFactory::AddObserver(
    ui::ContextFactoryObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TestImageTransportFactory::RemoveObserver(
    ui::ContextFactoryObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool TestImageTransportFactory::SyncTokensRequiredForDisplayCompositor() {
  return true;
}

std::unique_ptr<ui::Reflector> TestImageTransportFactory::CreateReflector(
    ui::Compositor* source,
    ui::Layer* target) {
  if (!enable_viz_)
    return std::make_unique<FakeReflector>();

  // TODO(crbug.com/601869): Reflector needs to be rewritten for viz.
  NOTIMPLEMENTED();
  return nullptr;
}

viz::FrameSinkId TestImageTransportFactory::AllocateFrameSinkId() {
  return frame_sink_id_allocator_.NextFrameSinkId();
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

ui::ContextFactoryPrivate*
TestImageTransportFactory::GetContextFactoryPrivate() {
  return this;
}

}  // namespace content
