// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_GPU_PROCESS_TRANSPORT_FACTORY_H_
#define CONTENT_BROWSER_COMPOSITOR_GPU_PROCESS_TRANSPORT_FACTORY_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/containers/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/public/cpp/gpu/command_buffer_metrics.h"
#include "services/viz/public/cpp/gpu/shared_worker_context_provider_factory.h"
#include "ui/compositor/compositor.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {
class SingleThreadTaskGraphRunner;
class SurfaceManager;
}

namespace gpu {
class GpuChannelEstablishFactory;
}

namespace viz {
class CompositingModeReporterImpl;
class RasterContextProvider;
class ServerSharedBitmapManager;
class SoftwareOutputDevice;
}

namespace viz {
class ContextProviderCommandBuffer;
}

namespace content {

class GpuProcessTransportFactory : public ui::ContextFactory,
                                   public ui::ContextFactoryPrivate,
                                   public ImageTransportFactory,
                                   public viz::ContextLostObserver {
 public:
  GpuProcessTransportFactory(
      gpu::GpuChannelEstablishFactory* gpu_channel_factory,
      viz::CompositingModeReporterImpl* compositing_mode_reporter,
      viz::ServerSharedBitmapManager* server_shared_bitmap_manager,
      scoped_refptr<base::SingleThreadTaskRunner> resize_task_runner);

  ~GpuProcessTransportFactory() override;

  // ui::ContextFactory implementation.
  void CreateLayerTreeFrameSink(
      base::WeakPtr<ui::Compositor> compositor) override;
  scoped_refptr<viz::ContextProvider> SharedMainThreadContextProvider()
      override;
  scoped_refptr<viz::RasterContextProvider>
  SharedMainThreadRasterContextProvider() override;

  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  void AddObserver(ui::ContextFactoryObserver* observer) override;
  void RemoveObserver(ui::ContextFactoryObserver* observer) override;
  bool SyncTokensRequiredForDisplayCompositor() override;

  // ui::ContextFactoryPrivate implementation.
  std::unique_ptr<ui::Reflector> CreateReflector(ui::Compositor* source,
                                                 ui::Layer* target) override;
  void RemoveReflector(ui::Reflector* reflector) override;
  void RemoveCompositor(ui::Compositor* compositor) override;
  viz::FrameSinkId AllocateFrameSinkId() override;
  viz::HostFrameSinkManager* GetHostFrameSinkManager() override;
  void SetDisplayVisible(ui::Compositor* compositor, bool visible) override;
  void ResizeDisplay(ui::Compositor* compositor,
                     const gfx::Size& size) override;
  void DisableSwapUntilResize(ui::Compositor* compositor) override;
  void SetDisplayColorMatrix(ui::Compositor* compositor,
                             const SkMatrix44& matrix) override;
  void SetDisplayColorSpace(ui::Compositor* compositor,
                            const gfx::ColorSpace& output_color_space,
                            float sdr_white_level) override;
  void SetDisplayVSyncParameters(ui::Compositor* compositor,
                                 base::TimeTicks timebase,
                                 base::TimeDelta interval) override;
  void IssueExternalBeginFrame(
      ui::Compositor* compositor,
      const viz::BeginFrameArgs& args,
      bool force,
      base::OnceCallback<void(const viz::BeginFrameAck&)> callback) override;
  void SetOutputIsSecure(ui::Compositor* compositor, bool secure) override;
  void AddVSyncParameterObserver(
      ui::Compositor* compositor,
      mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer)
      override;
  void SetDisplayTransformHint(ui::Compositor* compositor,
                               gfx::OverlayTransform transform) override;

  // ImageTransportFactory implementation.
  void DisableGpuCompositing() override;
  ui::ContextFactory* GetContextFactory() override;
  ui::ContextFactoryPrivate* GetContextFactoryPrivate() override;

 private:
  struct PerCompositorData;

  scoped_refptr<viz::RasterContextProvider> shared_worker_context_provider();

  PerCompositorData* CreatePerCompositorData(ui::Compositor* compositor);
  std::unique_ptr<viz::SoftwareOutputDevice> CreateSoftwareOutputDevice(
      gfx::AcceleratedWidget widget,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  void EstablishedGpuChannel(
      base::WeakPtr<ui::Compositor> compositor,
      bool use_gpu_compositing,
      scoped_refptr<gpu::GpuChannelHost> established_channel_host);

  void DisableGpuCompositing(ui::Compositor* guilty_compositor);

  void OnLostMainThreadSharedContext();

  // viz::ContextLostObserver implementation.
  void OnContextLost() override;

  scoped_refptr<viz::ContextProviderCommandBuffer> CreateContextCommon(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
      gpu::SurfaceHandle surface_handle,
      bool need_alpha_channel,
      bool need_stencil_bits,
      bool support_locking,
      bool support_gles2_interface,
      bool support_raster_interface,
      bool support_grcontext,
      viz::command_buffer_metrics::ContextType type);

  viz::FrameSinkIdAllocator frame_sink_id_allocator_;

  // Depends on SurfaceManager.
  typedef std::map<ui::Compositor*, std::unique_ptr<PerCompositorData>>
      PerCompositorDataMap;
  PerCompositorDataMap per_compositor_data_;

  scoped_refptr<viz::ContextProviderCommandBuffer> shared_main_thread_contexts_;
  base::ObserverList<ui::ContextFactoryObserver>::Unchecked observer_list_;
  scoped_refptr<base::SingleThreadTaskRunner> resize_task_runner_;
  std::unique_ptr<cc::SingleThreadTaskGraphRunner> task_graph_runner_;
  viz::SharedWorkerContextProviderFactory
      shared_worker_context_provider_factory_;

  bool is_gpu_compositing_disabled_ = false;
  bool disable_frame_rate_limit_ = false;
  bool wait_for_all_pipeline_stages_before_draw_ = false;

  gpu::GpuChannelEstablishFactory* const gpu_channel_factory_;
  // Service-side impl that controls the compositing mode based on what mode the
  // display compositors are using.
  viz::CompositingModeReporterImpl* const compositing_mode_reporter_;
  // Manages a mapping of SharedBitmapId to shared memory objects.
  viz::ServerSharedBitmapManager* const server_shared_bitmap_manager_;

  base::WeakPtrFactory<GpuProcessTransportFactory> callback_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GpuProcessTransportFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_GPU_PROCESS_TRANSPORT_FACTORY_H_
