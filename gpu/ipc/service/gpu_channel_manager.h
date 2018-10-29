// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_CHANNEL_MANAGER_H_
#define GPU_IPC_SERVICE_GPU_CHANNEL_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/gr_cache_controller.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
#include "gpu/command_buffer/service/passthrough_discardable_manager.h"
#include "gpu/command_buffer/service/raster_decoder_context_state.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/command_buffer/service/shader_translator_cache.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface.h"
#include "url/gurl.h"

namespace gl {
class GLShareGroup;
}

namespace gpu {

class SharedImageManager;
struct GpuPreferences;
struct SyncToken;
class GpuChannel;
class GpuChannelManagerDelegate;
class GpuMemoryBufferFactory;
class GpuWatchdogThread;
class MailboxManager;
class Scheduler;
class SyncPointManager;
struct VideoMemoryUsageStats;

namespace gles2 {
class Outputter;
class ProgramCache;
}  // namespace gles2

// A GpuChannelManager is a thread responsible for issuing rendering commands
// managing the lifetimes of GPU channels and forwarding IPC requests from the
// browser process to them based on the corresponding renderer ID.
class GPU_IPC_SERVICE_EXPORT GpuChannelManager
    : public raster::GrShaderCache::Client {
 public:
  GpuChannelManager(const GpuPreferences& gpu_preferences,
                    GpuChannelManagerDelegate* delegate,
                    GpuWatchdogThread* watchdog,
                    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                    Scheduler* scheduler,
                    SyncPointManager* sync_point_manager,
                    GpuMemoryBufferFactory* gpu_memory_buffer_factory,
                    const GpuFeatureInfo& gpu_feature_info,
                    GpuProcessActivityFlags activity_flags,
                    scoped_refptr<gl::GLSurface> default_offscreen_surface);
  ~GpuChannelManager() override;

  GpuChannelManagerDelegate* delegate() const { return delegate_; }
  GpuWatchdogThread* watchdog() const { return watchdog_; }

  GpuChannel* EstablishChannel(int client_id,
                               uint64_t client_tracing_id,
                               bool is_gpu_host,
                               bool cache_shaders_on_disk);

  void PopulateShaderCache(int32_t client_id,
                           const std::string& key,
                           const std::string& program);
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id,
                              const SyncToken& sync_token);
#if defined(OS_ANDROID)
  void WakeUpGpu();
#endif
  void DestroyAllChannels();

  // Remove the channel for a particular renderer.
  void RemoveChannel(int client_id);

  void LoseAllContexts();
  void MaybeExitOnContextLost();

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

#if defined(OS_ANDROID)
  void DidAccessGpu();
  void OnBackgroundCleanup();
#endif

  void OnApplicationBackgrounded();

  bool is_exiting_for_lost_context() { return exiting_for_lost_context_; }

  MailboxManager* mailbox_manager() { return mailbox_manager_.get(); }

  gl::GLShareGroup* share_group() const { return share_group_.get(); }

  SyncPointManager* sync_point_manager() const { return sync_point_manager_; }

  SharedImageManager* shared_image_manager() { return &shared_image_manager_; }

  // Retrieve GPU Resource consumption statistics for the task manager
  void GetVideoMemoryUsageStats(
      VideoMemoryUsageStats* video_memory_usage_stats) const;

  scoped_refptr<raster::RasterDecoderContextState> GetRasterDecoderContextState(
      ContextResult* result);
  void ScheduleGrContextCleanup();
  raster::GrShaderCache* gr_shader_cache() {
    return gr_shader_cache_ ? &*gr_shader_cache_ : nullptr;
  }

  // raster::GrShaderCache::Client implementation.
  void StoreShader(const std::string& key, const std::string& shader) override;

  void InternalDestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id, int client_id);
  void InternalDestroyGpuMemoryBufferOnIO(gfx::GpuMemoryBufferId id,
                                          int client_id);
#if defined(OS_ANDROID)
  void ScheduleWakeUpGpu();
  void DoWakeUpGpu();
#endif

  void HandleMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // These objects manage channels to individual renderer processes. There is
  // one channel for each renderer process that has connected to this GPU
  // process.
  std::unordered_map<int32_t, std::unique_ptr<GpuChannel>> gpu_channels_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  const GpuPreferences gpu_preferences_;
  const GpuDriverBugWorkarounds gpu_driver_bug_workarounds_;

  GpuChannelManagerDelegate* const delegate_;

  GpuWatchdogThread* watchdog_;

  scoped_refptr<gl::GLShareGroup> share_group_;

  std::unique_ptr<MailboxManager> mailbox_manager_;
  std::unique_ptr<gles2::Outputter> outputter_;
  Scheduler* scheduler_;
  // SyncPointManager guaranteed to outlive running MessageLoop.
  SyncPointManager* sync_point_manager_;
  std::unique_ptr<gles2::ProgramCache> program_cache_;
  gles2::ShaderTranslatorCache shader_translator_cache_;
  gles2::FramebufferCompletenessCache framebuffer_completeness_cache_;
  scoped_refptr<gl::GLSurface> default_offscreen_surface_;
  GpuMemoryBufferFactory* const gpu_memory_buffer_factory_;
  GpuFeatureInfo gpu_feature_info_;
  ServiceDiscardableManager discardable_manager_;
  PassthroughDiscardableManager passthrough_discardable_manager_;
  SharedImageManager shared_image_manager_;
#if defined(OS_ANDROID)
  // Last time we know the GPU was powered on. Global for tracking across all
  // transport surfaces.
  base::TimeTicks last_gpu_access_time_;
  base::TimeTicks begin_wake_up_time_;
#endif

  // Set during intentional GPU process shutdown.
  bool exiting_for_lost_context_;

  // Flags which indicate GPU process activity. Read by the browser process
  // on GPU process crash.
  GpuProcessActivityFlags activity_flags_;

  base::MemoryPressureListener memory_pressure_listener_;

  // The RasterDecoderContextState is shared across all RasterDecoders. Note
  // that this class needs to be ref-counted to conveniently manage the lifetime
  // of the shared context in the case of a context loss. While the
  // GpuChannelManager strictly outlives the RasterDecoders, in the event of a
  // context loss the clients need to re-create the GpuChannel and command
  // buffers once notified. In this interim state we can have multiple instances
  // of the RasterDecoderContextState, for the lost and recovered clients. In
  // order to avoid having the GpuChannelManager keep the lost context state
  // alive until all clients have recovered, we use a ref-counted object and
  // allow the decoders to manage its lifetime.
  base::Optional<raster::GrShaderCache> gr_shader_cache_;
  base::Optional<raster::GrCacheController> gr_cache_controller_;
  scoped_refptr<raster::RasterDecoderContextState>
      raster_decoder_context_state_;

  // Member variables should appear before the WeakPtrFactory, to ensure
  // that any WeakPtrs to Controller are invalidated before its members
  // variable's destructors are executed, rendering them invalid.
  base::WeakPtrFactory<GpuChannelManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelManager);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_CHANNEL_MANAGER_H_
