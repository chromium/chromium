// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_CHANNEL_MANAGER_H_
#define GPU_IPC_SERVICE_GPU_CHANNEL_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/shm_count.h"
#include "gpu/command_buffer/service/gr_cache_controller.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/passthrough_discardable_manager.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shader_translator_cache.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/common/gpu_disk_cache_type.h"
#include "gpu/ipc/common/gpu_peak_memory.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface.h"
#include "url/gurl.h"

namespace base::trace_event {
class TracedValue;
}  // namespace base::trace_event

namespace gfx {
struct GpuExtraInfo;
}

namespace gl {
class GLShareGroup;
}

namespace gpu {

class BuiltInShaderCacheWriter;
class DawnContextProvider;
class ImageDecodeAcceleratorWorker;
struct GpuPreferences;
class GpuChannel;
class GpuChannelManagerDelegate;
class GpuMemoryBufferFactory;
class GpuWatchdogThread;
class Scheduler;
class SharedImageManager;
class SyncPointManager;
struct VideoMemoryUsageStats;

namespace gles2 {
class Outputter;
class ProgramCache;
}  // namespace gles2

namespace webgpu {
class DawnCachingInterfaceFactory;
}  // namespace webgpu

// A GpuChannelManager is a thread responsible for issuing rendering commands
// managing the lifetimes of GPU channels and forwarding IPC requests from the
// browser process to them based on the corresponding renderer ID.
class GPU_IPC_SERVICE_EXPORT GpuChannelManager
    : public raster::GrShaderCache::Client {
 public:
  using OnMemoryAllocatedChangeCallback =
      base::OnceCallback<void(gpu::CommandBufferId id,
                              uint64_t old_size,
                              uint64_t new_size,
                              gpu::GpuPeakMemoryAllocationSource source)>;

  GpuChannelManager(
      const GpuPreferences& gpu_preferences,
      GpuChannelManagerDelegate* delegate,
      GpuWatchdogThread* watchdog,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      Scheduler* scheduler,
      SyncPointManager* sync_point_manager,
      SharedImageManager* shared_image_manager,
      GpuMemoryBufferFactory* gpu_memory_buffer_factory,
      const GpuFeatureInfo& gpu_feature_info,
      GpuProcessShmCount use_shader_cache_shm_count,
      scoped_refptr<gl::GLSurface> default_offscreen_surface,
      ImageDecodeAcceleratorWorker* image_decode_accelerator_worker,
      viz::VulkanContextProvider* vulkan_context_provider = nullptr,
      viz::MetalContextProvider* metal_context_provider = nullptr,
      DawnContextProvider* dawn_context_provider = nullptr,
      webgpu::DawnCachingInterfaceFactory* dawn_caching_interface_factory =
          nullptr);

  GpuChannelManager(const GpuChannelManager&) = delete;
  GpuChannelManager& operator=(const GpuChannelManager&) = delete;

  ~GpuChannelManager() override;

  GpuChannelManagerDelegate* delegate() const { return delegate_; }
  GpuWatchdogThread* watchdog() const { return watchdog_; }

  GpuChannel* EstablishChannel(
      const base::UnguessableToken& channel_token,
      int client_id,
      uint64_t client_tracing_id,
      bool is_gpu_host,
      const gfx::GpuExtraInfo& gpu_extra_info,
      gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory);

  void SetChannelClientPid(int client_id, base::ProcessId client_pid);
  void SetChannelDiskCacheHandle(int client_id,
                                 const gpu::GpuDiskCacheHandle& handle);
  void OnDiskCacheHandleDestoyed(const gpu::GpuDiskCacheHandle& handle);

  void PopulateCache(const gpu::GpuDiskCacheHandle& handle,
                     const std::string& key,
                     const std::string& program);
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id, int client_id);
#if BUILDFLAG(IS_ANDROID)
  void WakeUpGpu();
#endif
  void DestroyAllChannels();

  // Remove the channel for a particular renderer.
  void RemoveChannel(int client_id);

  void OnContextLost(int context_lost_count,
                     bool synthetic_loss,
                     error::ContextLostReason context_lost_reason);

  const GpuPreferences& gpu_preferences() const { return gpu_preferences_; }
  const GpuDriverBugWorkarounds& gpu_driver_bug_workarounds() const {
    return gpu_driver_bug_workarounds_;
  }
  const GpuFeatureInfo& gpu_feature_info() const { return gpu_feature_info_; }
  ServiceDiscardableManager* discardable_manager() {
    return &discardable_manager_;
  }
  PassthroughDiscardableManager* passthrough_discardable_manager() {
    return &passthrough_discardable_manager_;
  }
  gles2::Outputter* outputter();
  gles2::ProgramCache* program_cache();
  gles2::ShaderTranslatorCache* shader_translator_cache() {
    return &shader_translator_cache_;
  }
  gles2::FramebufferCompletenessCache* framebuffer_completeness_cache() {
    return &framebuffer_completeness_cache_;
  }

  GpuChannel* LookupChannel(int32_t client_id) const;

  gl::GLSurface* default_offscreen_surface() const {
    return default_offscreen_surface_.get();
  }

  GpuMemoryBufferFactory* gpu_memory_buffer_factory() {
    return gpu_memory_buffer_factory_;
  }

  MemoryTracker::Observer* peak_memory_monitor() {
    return &peak_memory_monitor_;
  }

  GpuProcessShmCount* use_shader_cache_shm_count() {
    return &use_shader_cache_shm_count_;
  }

#if BUILDFLAG(IS_ANDROID)
  void DidAccessGpu();
  void OnBackgroundCleanup();
#endif

  void OnApplicationBackgrounded();
  void OnApplicationForegounded();
  bool application_backgrounded() const { return application_backgrounded_; }
  // Make sure that delayed cleanup is happening now. Expensive.
  void PerformImmediateCleanup();

  gl::GLShareGroup* share_group() const { return share_group_.get(); }

  SyncPointManager* sync_point_manager() const { return sync_point_manager_; }

  SharedImageManager* shared_image_manager() const {
    return shared_image_manager_;
  }

  Scheduler* scheduler() const { return scheduler_; }

  bool use_passthrough_cmd_decoder() const {
    return gpu_preferences_.use_passthrough_cmd_decoder &&
           gles2::PassthroughCommandDecoderSupported();
  }

  // Retrieve GPU Resource consumption statistics for the task manager
  void GetVideoMemoryUsageStats(
      VideoMemoryUsageStats* video_memory_usage_stats) const;

  // Starts tracking the peak memory across all MemoryTrackers for
  // |sequence_num|. Repeated calls with the same value are ignored.
  void StartPeakMemoryMonitor(uint32_t sequence_num);

  // Ends the tracking for |sequence_num| and returns the peak memory per
  // allocation source. Along with the total |out_peak_memory|.
  base::flat_map<GpuPeakMemoryAllocationSource, uint64_t> GetPeakMemoryUsage(
      uint32_t sequence_num,
      uint64_t* out_peak_memory);

  scoped_refptr<SharedContextState> GetSharedContextState(
      ContextResult* result);
  void ScheduleGrContextCleanup();
  raster::GrShaderCache* gr_shader_cache() {
    return gr_shader_cache_ ? &*gr_shader_cache_ : nullptr;
  }

  webgpu::DawnCachingInterfaceFactory* dawn_caching_interface_factory() {
    return dawn_caching_interface_factory_.get();
  }

  // raster::GrShaderCache::Client implementation.
  void StoreShader(const std::string& key, const std::string& shader) override;

  void SetImageDecodeAcceleratorWorkerForTesting(
      ImageDecodeAcceleratorWorker* worker);

  void LoseAllContexts();

  SharedContextState::ContextLostCallback GetContextLostCallback();
  GpuChannelManager::OnMemoryAllocatedChangeCallback
  GetOnMemoryAllocatedChangeCallback();

 private:
  friend class GpuChannelManagerTest;

  // Observes changes in GPU memory, and tracks the peak usage for clients. The
  // client is responsible for providing a unique |sequence_num| for each time
  // period in which it wishes to track memory usage.
  class GPU_IPC_SERVICE_EXPORT GpuPeakMemoryMonitor
      : public MemoryTracker::Observer {
   public:
    GpuPeakMemoryMonitor(
        GpuChannelManager* channel_manager,
        scoped_refptr<base::SingleThreadTaskRunner> task_runner);

    GpuPeakMemoryMonitor(const GpuPeakMemoryMonitor&) = delete;
    GpuPeakMemoryMonitor& operator=(const GpuPeakMemoryMonitor&) = delete;

    ~GpuPeakMemoryMonitor() override;

    base::flat_map<GpuPeakMemoryAllocationSource, uint64_t> GetPeakMemoryUsage(
        uint32_t sequence_num,
        uint64_t* out_peak_memory);
    void StartGpuMemoryTracking(uint32_t sequence_num);
    void StopGpuMemoryTracking(uint32_t sequence_num);

    base::WeakPtr<MemoryTracker::Observer> GetWeakPtr();
    void InvalidateWeakPtrs();

   private:
    struct SequenceTracker {
     public:
      SequenceTracker(uint64_t current_memory,
                      base::flat_map<GpuPeakMemoryAllocationSource, uint64_t>
                          current_memory_per_source);
      SequenceTracker(const SequenceTracker&);
      ~SequenceTracker();

      uint64_t initial_memory_ = 0u;
      uint64_t total_memory_ = 0u;
      base::flat_map<GpuPeakMemoryAllocationSource, uint64_t>
          initial_memory_per_source_;
      base::flat_map<GpuPeakMemoryAllocationSource, uint64_t>
          peak_memory_per_source_;
    };
    std::unique_ptr<base::trace_event::TracedValue> StartTrackingTracedValue();
    std::unique_ptr<base::trace_event::TracedValue> StopTrackingTracedValue(
        SequenceTracker& sequence);
    // MemoryTracker::Observer:
    void OnMemoryAllocatedChange(
        CommandBufferId id,
        uint64_t old_size,
        uint64_t new_size,
        GpuPeakMemoryAllocationSource source =
            GpuPeakMemoryAllocationSource::UNKNOWN) override;

    // Tracks all currently requested sequences mapped to the peak memory seen.
    base::flat_map<uint32_t, SequenceTracker> sequence_trackers_;

    // Tracks the total current memory across all MemoryTrackers.
    uint64_t current_memory_ = 0u;

    base::flat_map<GpuPeakMemoryAllocationSource, uint64_t>
        current_memory_per_source_;

    base::WeakPtrFactory<GpuPeakMemoryMonitor> weak_factory_;
  };

#if BUILDFLAG(IS_ANDROID)
  void ScheduleWakeUpGpu();
  void DoWakeUpGpu();
#endif

  void HandleMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // These objects manage channels to individual renderer processes. There is
  // one channel for each renderer process that has connected to this GPU
  // process.
  base::flat_map<int32_t, std::unique_ptr<GpuChannel>> gpu_channels_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  const GpuPreferences gpu_preferences_;
  const GpuDriverBugWorkarounds gpu_driver_bug_workarounds_;

  const raw_ptr<GpuChannelManagerDelegate> delegate_;

  raw_ptr<GpuWatchdogThread> watchdog_;

  scoped_refptr<gl::GLShareGroup> share_group_;

#if BUILDFLAG(IS_MAC)
  std::unique_ptr<BuiltInShaderCacheWriter> shader_cache_writer_;
#endif

  std::unique_ptr<gles2::Outputter> outputter_;
  raw_ptr<Scheduler> scheduler_;
  // SyncPointManager guaranteed to outlive running MessageLoop.
  const raw_ptr<SyncPointManager> sync_point_manager_;
  const raw_ptr<SharedImageManager> shared_image_manager_;
  std::unique_ptr<gles2::ProgramCache> program_cache_;
  gles2::ShaderTranslatorCache shader_translator_cache_;
  gles2::FramebufferCompletenessCache framebuffer_completeness_cache_;
  scoped_refptr<gl::GLSurface> default_offscreen_surface_;
  const raw_ptr<GpuMemoryBufferFactory> gpu_memory_buffer_factory_;
  GpuFeatureInfo gpu_feature_info_;
  ServiceDiscardableManager discardable_manager_;
  PassthroughDiscardableManager passthrough_discardable_manager_;
#if BUILDFLAG(IS_ANDROID)
  // Last time we know the GPU was powered on. Global for tracking across all
  // transport surfaces.
  base::TimeTicks last_gpu_access_time_;
  base::TimeTicks begin_wake_up_time_;
#endif

  raw_ptr<ImageDecodeAcceleratorWorker> image_decode_accelerator_worker_ =
      nullptr;

  // A count in shared memory that's non-zero for the duration of loading
  // shaders. Read by the browser process on GPU process crash.
  GpuProcessShmCount use_shader_cache_shm_count_;

  base::MemoryPressureListener memory_pressure_listener_;

  // The SharedContextState is shared across all RasterDecoders. Note
  // that this class needs to be ref-counted to conveniently manage the lifetime
  // of the shared context in the case of a context loss. While the
  // GpuChannelManager strictly outlives the RasterDecoders, in the event of a
  // context loss the clients need to re-create the GpuChannel and command
  // buffers once notified. In this interim state we can have multiple instances
  // of the SharedContextState, for the lost and recovered clients. In
  // order to avoid having the GpuChannelManager keep the lost context state
  // alive until all clients have recovered, we use a ref-counted object and
  // allow the decoders to manage its lifetime.
  std::optional<raster::GrShaderCache> gr_shader_cache_;
  scoped_refptr<SharedContextState> shared_context_state_;

  raw_ptr<webgpu::DawnCachingInterfaceFactory> dawn_caching_interface_factory_;

  // With --enable-vulkan, |vulkan_context_provider_| will be set from
  // viz::GpuServiceImpl. The raster decoders will use it for rasterization if
  // features::Vulkan is used.
  raw_ptr<viz::VulkanContextProvider> vulkan_context_provider_ = nullptr;

  // If features::SkiaGraphite, |metal_context_provider_| will be set from
  // viz::GpuServiceImpl. The raster decoders may use it for rasterization.
  raw_ptr<viz::MetalContextProvider> metal_context_provider_ = nullptr;

  // With features::SkiaGraphite, |dawn_context_provider_| will be set from
  // viz::GpuServiceImpl. The raster decoders may use it for rasterization.
  raw_ptr<DawnContextProvider> dawn_context_provider_ = nullptr;

  GpuPeakMemoryMonitor peak_memory_monitor_;

  // Creation time of GpuChannelManger.
  const base::TimeTicks creation_time_ = base::TimeTicks::Now();

  // Context lost time since creation of |GpuChannelManger|.
  base::TimeDelta context_lost_time_;

  // Count of context lost.
  int context_lost_count_ = 0;

  bool application_backgrounded_ = false;

  THREAD_CHECKER(thread_checker_);

  // Member variables should appear before the WeakPtrFactory, to ensure
  // that any WeakPtrs to Controller are invalidated before its members
  // variable's destructors are executed, rendering them invalid.
  base::WeakPtrFactory<GpuChannelManager> weak_factory_{this};
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_CHANNEL_MANAGER_H_
