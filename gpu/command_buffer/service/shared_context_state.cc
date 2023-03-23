// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_context_state.h"

#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
#include "gpu/command_buffer/service/service_transfer_cache.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/skia_limits.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/vulkan/buildflags.h"
#include "skia/buildflags.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/create_gr_gl_interface.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include <vulkan/vulkan.h>

#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/service/external_semaphore_pool.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "gpu/vulkan/fuchsia/vulkan_fuchsia_ext.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "components/viz/common/gpu/metal_context_provider.h"
#endif

#if BUILDFLAG(SKIA_USE_DAWN)
#include "components/viz/common/gpu/dawn_context_provider.h"
#endif

namespace {
static constexpr size_t kInitialScratchDeserializationBufferSize = 1024;

size_t MaxNumSkSurface() {
  static constexpr size_t kNormalMaxNumSkSurface = 16;
#if BUILDFLAG(IS_ANDROID)
  static constexpr size_t kLowEndMaxNumSkSurface = 4;
  if (base::SysInfo::IsLowEndDevice()) {
    return kLowEndMaxNumSkSurface;
  } else {
    return kNormalMaxNumSkSurface;
  }
#else
  return kNormalMaxNumSkSurface;
#endif
}
}

namespace gpu {

void SharedContextState::compileError(const char* shader, const char* errors) {
  if (!context_lost()) {
    LOG(ERROR) << "Skia shader compilation error\n"
               << "------------------------\n"
               << shader << "\nErrors:\n"
               << errors;
  }
}

SharedContextState::MemoryTrackerObserver::MemoryTrackerObserver(
    base::WeakPtr<gpu::MemoryTracker::Observer> peak_memory_monitor)
    : peak_memory_monitor_(peak_memory_monitor) {}

SharedContextState::MemoryTrackerObserver::~MemoryTrackerObserver() {
  DCHECK(!size_);
}

void SharedContextState::MemoryTrackerObserver::OnMemoryAllocatedChange(
    CommandBufferId id,
    uint64_t old_size,
    uint64_t new_size,
    GpuPeakMemoryAllocationSource source) {
  size_ += new_size - old_size;
  if (source == GpuPeakMemoryAllocationSource::UNKNOWN)
    source = GpuPeakMemoryAllocationSource::SHARED_CONTEXT_STATE;
  if (peak_memory_monitor_) {
    peak_memory_monitor_->OnMemoryAllocatedChange(id, old_size, new_size,
                                                  source);
  }
}

base::AtomicSequenceNumber g_next_command_buffer_id;

SharedContextState::MemoryTracker::MemoryTracker(Observer* observer)
    : command_buffer_id_(gpu::CommandBufferId::FromUnsafeValue(
          g_next_command_buffer_id.GetNext() + 1)),
      client_tracing_id_(base::trace_event::MemoryDumpManager::GetInstance()
                             ->GetTracingProcessId()),
      observer_(observer) {}

SharedContextState::MemoryTracker::~MemoryTracker() {
  DCHECK(!size_);
}

void SharedContextState::MemoryTracker::TrackMemoryAllocatedChange(
    int64_t delta) {
  DCHECK(delta >= 0 || size_ >= static_cast<uint64_t>(-delta));
  uint64_t old_size = size_;
  size_ += delta;
  DCHECK(observer_);
  observer_->OnMemoryAllocatedChange(command_buffer_id_, old_size, size_,
                                     gpu::GpuPeakMemoryAllocationSource::SKIA);
}

uint64_t SharedContextState::MemoryTracker::GetSize() const {
  return size_;
}

uint64_t SharedContextState::MemoryTracker::ClientTracingId() const {
  return client_tracing_id_;
}

int SharedContextState::MemoryTracker::ClientId() const {
  return gpu::ChannelIdFromCommandBufferId(command_buffer_id_);
}

uint64_t SharedContextState::MemoryTracker::ContextGroupTracingId() const {
  return command_buffer_id_.GetUnsafeValue();
}

SharedContextState::SharedContextState(
    scoped_refptr<gl::GLShareGroup> share_group,
    scoped_refptr<gl::GLSurface> surface,
    scoped_refptr<gl::GLContext> context,
    bool use_virtualized_gl_contexts,
    ContextLostCallback context_lost_callback,
    GrContextType gr_context_type,
    viz::VulkanContextProvider* vulkan_context_provider,
    viz::MetalContextProvider* metal_context_provider,
    viz::DawnContextProvider* dawn_context_provider,
    base::WeakPtr<gpu::MemoryTracker::Observer> peak_memory_monitor,
    bool created_on_compositor_gpu_thread)
    : use_virtualized_gl_contexts_(use_virtualized_gl_contexts),
      context_lost_callback_(std::move(context_lost_callback)),
      gr_context_type_(gr_context_type),
      memory_tracker_observer_(peak_memory_monitor),
      memory_tracker_(&memory_tracker_observer_),
      memory_type_tracker_(&memory_tracker_),
      vk_context_provider_(vulkan_context_provider),
      metal_context_provider_(metal_context_provider),
      dawn_context_provider_(dawn_context_provider),
      created_on_compositor_gpu_thread_(created_on_compositor_gpu_thread),
      share_group_(std::move(share_group)),
      context_(context),
      real_context_(std::move(context)),
      surface_(std::move(surface)),
      sk_surface_cache_(MaxNumSkSurface()) {
  static crash_reporter::CrashKeyString<16> crash_key("gr-context-type");
  crash_key.Set(
      base::StringPrintf("%u", static_cast<uint32_t>(gr_context_type_)));
  // If |gr_context_type_| is not GL, then initialize |gr_context_| here. In
  // the case of GL, |gr_context_| will be initialized in InitializeGrContext.
  // Note that if |gr_context_| is not GL and also not initialized here (e.g,
  // due to vk/metal/dawn_context_provider_ being nullptr), then
  // InitializeGrContext will fail.
  switch (gr_context_type_) {
    case GrContextType::kGL:
      break;
    case GrContextType::kVulkan:
      if (vk_context_provider_) {
#if BUILDFLAG(ENABLE_VULKAN)
        external_semaphore_pool_ =
            std::make_unique<ExternalSemaphorePool>(this);
#endif
        use_virtualized_gl_contexts_ = false;
      }
      break;
    case GrContextType::kMetal:
      if (metal_context_provider_) {
#if BUILDFLAG(IS_APPLE)
        gr_context_ = metal_context_provider_->GetGrContext();
#endif
        use_virtualized_gl_contexts_ = false;
        DCHECK(gr_context_);
      }
      break;
    case GrContextType::kDawn:
      if (dawn_context_provider_) {
#if BUILDFLAG(SKIA_USE_DAWN)
        gr_context_ = dawn_context_provider_->GetGrContext();
#endif
        use_virtualized_gl_contexts_ = false;
        DCHECK(gr_context_);
      }
      break;
  }

  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "SharedContextState",
        base::SingleThreadTaskRunner::GetCurrentDefault());

    // Create |gr_cache_controller_| only if we have task runner.
    gr_cache_controller_.emplace(this);
  }
  // Initialize the scratch buffer to some small initial size.
  scratch_deserialization_buffer_.resize(
      kInitialScratchDeserializationBufferSize);
}

SharedContextState::~SharedContextState() {
  DCHECK(sk_surface_cache_.empty());
  // Delete the transfer cache first: that way, destruction callbacks for image
  // entries can use *|this| to make the context current and do GPU clean up.
  // The context should be current so that texture deletes that result from
  // destroying the cache happen in the right context (unless the context is
  // lost in which case we don't delete the textures).
  DCHECK(IsCurrent(nullptr) || context_lost());
  transfer_cache_.reset();

#if BUILDFLAG(ENABLE_VULKAN)
  external_semaphore_pool_.reset();
#endif

  // We should have the last ref on this GrContext to ensure we're not holding
  // onto any skia objects using this context. Note that some tests don't run
  // InitializeGrContext(), so |owned_gr_context_| is not expected to be
  // initialized.
  DCHECK(!owned_gr_context_ || owned_gr_context_->unique());

  // GPU memory allocations except skia_gr_cache_size_ tracked by this
  // memory_tracker_observer_ should have been released.
  DCHECK_EQ(skia_gr_cache_size_, memory_tracker_observer_.GetMemoryUsage());
  // gr_context_ and all resources owned by it will be released soon, so set it
  // to null, and UpdateSkiaOwnedMemorySize() will update skia memory usage to
  // 0, to ensure that PeakGpuMemoryMonitor sees 0 allocated memory.
  gr_context_ = nullptr;
  UpdateSkiaOwnedMemorySize();

  // Delete the GrContext. This will either do cleanup if the context is
  // current, or the GrContext was already abandoned if the GLContext was lost.
  owned_gr_context_.reset();

  // |surface_| needs to be destroyed while there is a current GL context if
  // using GL, as some implementations make calls to the GL bindings. Any such
  // implementations will themselves ensure that the context is current in their
  // destructor (we cannot blindly make the context current here, as there are
  // other implementations that crash if the context is made current at this
  // point :\). However, we drop our reference to |surface_| before releasing
  // the context below so that the release has the intended effect.
  last_current_surface_ = nullptr;
  surface_.reset();

  if (context_->IsCurrent(nullptr))
    context_->ReleaseCurrent(nullptr);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

bool SharedContextState::InitializeGrContext(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    gpu::raster::GrShaderCache* cache,
    GpuProcessActivityFlags* activity_flags,
    gl::ProgressReporter* progress_reporter) {
  progress_reporter_ = progress_reporter;
  gr_shader_cache_ = cache;

#if BUILDFLAG(IS_APPLE)
  if (metal_context_provider_)
    metal_context_provider_->SetProgressReporter(progress_reporter);
#endif

  size_t max_resource_cache_bytes;
  size_t glyph_cache_max_texture_bytes;
  DetermineGrCacheLimitsFromAvailableMemory(&max_resource_cache_bytes,
                                            &glyph_cache_max_texture_bytes);

  // If you make any changes to the GrContext::Options here that could
  // affect text rendering, make sure to match the capabilities initialized
  // in GetCapabilities and ensuring these are also used by the
  // PaintOpBufferSerializer.
  GrContextOptions options = GetDefaultGrContextOptions(gr_context_type_);
  options.fPersistentCache = cache;
  options.fShaderErrorHandler = this;
  if (gpu_preferences.force_max_texture_size)
    options.fMaxTextureSizeOverride = gpu_preferences.force_max_texture_size;

  if (base::FeatureList::IsEnabled(features::kReduceOpsTaskSplitting) &&
      !workarounds.disable_skia_reduce_ops_task_splitting) {
    options.fReduceOpsTaskSplitting = GrContextOptions::Enable::kYes;
  } else {
    options.fReduceOpsTaskSplitting = GrContextOptions::Enable::kNo;
  }

  if (gr_context_type_ == GrContextType::kGL) {
    DCHECK(context_->IsCurrent(nullptr));
    constexpr bool use_version_es2 = false;
    sk_sp<GrGLInterface> gr_gl_interface(gl::init::CreateGrGLInterface(
        *context_->GetVersionInfo(), use_version_es2, progress_reporter));
    if (!gr_gl_interface) {
      LOG(ERROR) << "OOP raster support disabled: GrGLInterface creation "
                    "failed.";
      return false;
    }

    if (activity_flags && cache) {
      // |activity_flags| is safe to capture here since it must outlive the
      // this context state.
      gr_gl_interface->fFunctions.fProgramBinary =
          [activity_flags](GrGLuint program, GrGLenum binaryFormat,
                           void* binary, GrGLsizei length) {
            GpuProcessActivityFlags::ScopedSetFlag scoped_set_flag(
                activity_flags, ActivityFlagsBase::FLAG_LOADING_PROGRAM_BINARY);
            glProgramBinary(program, binaryFormat, binary, length);
          };
    }
    options.fDriverBugWorkarounds =
        GrDriverBugWorkarounds(workarounds.ToIntSet());
    options.fAvoidStencilBuffers = workarounds.avoid_stencil_buffers;
    if (workarounds.disable_program_disk_cache) {
      options.fShaderCacheStrategy =
          GrContextOptions::ShaderCacheStrategy::kBackendSource;
    }
    options.fPreferExternalImagesOverES3 = true;

    if (gl::GetGLImplementation() == gl::kGLImplementationStubGL) {
      // gl::kGLImplementationStubGL doesn't implement enough functions for
      // successful GrContext::MakeGL initialization. Fallback to mock context
      // instead.
      GrMockOptions mock_options;
      owned_gr_context_ = GrDirectContext::MakeMock(&mock_options, options);
      DCHECK(owned_gr_context_);
    } else {
      owned_gr_context_ =
          GrDirectContext::MakeGL(std::move(gr_gl_interface), options);
    }

    gr_context_ = owned_gr_context_.get();
  } else if (gr_context_type_ == GrContextType::kVulkan) {
#if BUILDFLAG(ENABLE_VULKAN)
    if (vk_context_provider_) {
      // TODO(vasilyt): Remove this if there is no problem with caching.
      if (!base::FeatureList::IsEnabled(
              features::kEnableGrShaderCacheForVulkan))
        options.fPersistentCache = nullptr;

      if (!vk_context_provider_->InitializeGrContext(options)) {
        LOG(ERROR) << "Failed to initialize GrContext for Vulkan.";
        return false;
      }
      gr_context_ = vk_context_provider_->GetGrContext();
      DCHECK(gr_context_);
    }
#endif
  }

  if (!gr_context_) {
    LOG(ERROR) << "OOP raster support disabled: GrContext creation failed.";
    return false;
  }

  gr_context_->setResourceCacheLimit(max_resource_cache_bytes);
  transfer_cache_ = std::make_unique<ServiceTransferCache>(gpu_preferences);
  return true;
}

bool SharedContextState::InitializeGL(
    const GpuPreferences& gpu_preferences,
    scoped_refptr<gles2::FeatureInfo> feature_info) {
  // We still need initialize GL when Vulkan is used, because RasterDecoder
  // depends on GL.
  // TODO(penghuang): don't initialize GL when RasterDecoder can work without
  // GL.
  if (IsGLInitialized()) {
    DCHECK(feature_info == feature_info_);
    DCHECK(context_state_);
    return true;
  }

  DCHECK(context_->IsCurrent(nullptr));

  bool use_passthrough_cmd_decoder =
      gpu_preferences.use_passthrough_cmd_decoder &&
      gles2::PassthroughCommandDecoderSupported();
  // Virtualized contexts don't work with passthrough command decoder.
  // See https://crbug.com/914976
  DCHECK(!use_passthrough_cmd_decoder || !use_virtualized_gl_contexts_);

  feature_info_ = std::move(feature_info);
  feature_info_->Initialize(feature_info_->context_type(),
                            use_passthrough_cmd_decoder,
                            gles2::DisallowedFeatures());

  auto* api = gl::g_current_gl_context;
  const GLint kGLES2RequiredMinimumVertexAttribs = 8u;
  GLint max_vertex_attribs = 0;
  api->glGetIntegervFn(GL_MAX_VERTEX_ATTRIBS, &max_vertex_attribs);
  if (max_vertex_attribs < kGLES2RequiredMinimumVertexAttribs) {
    LOG(ERROR)
        << "SharedContextState::InitializeGL failure max_vertex_attribs : "
        << max_vertex_attribs << " is less that minimum required : "
        << kGLES2RequiredMinimumVertexAttribs;
    feature_info_ = nullptr;
    return false;
  }

  const GLint kGLES2RequiredMinimumTextureUnits = 8u;
  GLint max_texture_units = 0;
  api->glGetIntegervFn(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max_texture_units);
  if (max_texture_units < kGLES2RequiredMinimumTextureUnits) {
    LOG(ERROR)
        << "SharedContextState::InitializeGL failure max_texture_units : "
        << max_texture_units << " is less that minimum required : "
        << kGLES2RequiredMinimumTextureUnits;
    feature_info_ = nullptr;
    return false;
  }

  context_state_ = std::make_unique<gles2::ContextState>(
      feature_info_.get(), false /* track_texture_and_sampler_units */);

  context_state_->set_api(api);
  context_state_->InitGenericAttribs(max_vertex_attribs);

  // Set all the default state because some GL drivers get it wrong.
  // TODO(backer): Not all of this state needs to be initialized. Reduce the set
  // if perf becomes a problem.
  context_state_->InitCapabilities(nullptr);
  context_state_->InitState(nullptr);

  // Init |sampler_units|, ContextState uses the size of it to reset sampler to
  // ground state.
  // TODO(penghuang): remove it when GrContext is created with ES 3.0.
  context_state_->sampler_units.resize(max_texture_units);

  GLenum driver_status = real_context_->CheckStickyGraphicsResetStatus();
  if (driver_status != GL_NO_ERROR) {
    // If the context was lost at any point before or during initialization,
    // the values queried from the driver could be bogus, and potentially
    // inconsistent between various ContextStates on the same underlying real
    // GL context. Make sure to report the failure early, to not allow
    // virtualized context switches in that case.
    LOG(ERROR) << "SharedContextState::InitializeGL failure driver error : "
               << driver_status;
    feature_info_ = nullptr;
    context_state_ = nullptr;
    return false;
  }

  if (use_virtualized_gl_contexts_) {
    auto virtual_context = base::MakeRefCounted<GLContextVirtual>(
        share_group_.get(), real_context_.get(),
        weak_ptr_factory_.GetWeakPtr());
    if (!virtual_context->Initialize(surface_.get(), gl::GLContextAttribs())) {
      LOG(ERROR) << "SharedContextState::InitializeGL failure Initialize "
                    "virtual context failed";
      feature_info_ = nullptr;
      context_state_ = nullptr;
      return false;
    }
    context_ = std::move(virtual_context);
    MakeCurrent(nullptr);
  }

  bool gl_supports_memory_object =
      gl::g_current_gl_driver->ext.b_GL_EXT_memory_object_fd ||
      gl::g_current_gl_driver->ext.b_GL_EXT_memory_object_win32 ||
      gl::g_current_gl_driver->ext.b_GL_ANGLE_memory_object_fuchsia;
  bool gl_supports_semaphore =
      gl::g_current_gl_driver->ext.b_GL_EXT_semaphore_fd ||
      gl::g_current_gl_driver->ext.b_GL_EXT_semaphore_win32 ||
      gl::g_current_gl_driver->ext.b_GL_ANGLE_semaphore_fuchsia;

  bool vk_supports_external_memory = false;
  bool vk_supports_external_semaphore = false;
  gpu::VulkanImplementationName vulkan_implementation =
      gpu::VulkanImplementationName::kNone;
#if BUILDFLAG(ENABLE_VULKAN)
  if (vk_context_provider_) {
    vulkan_implementation =
        vk_context_provider_->GetVulkanImplementation()->use_swiftshader()
            ? gpu::VulkanImplementationName::kSwiftshader
            : gpu::VulkanImplementationName::kNative;
    const auto& extensions =
        vk_context_provider_->GetDeviceQueue()->enabled_extensions();
#if BUILDFLAG(IS_WIN)
    vk_supports_external_memory =
        gfx::HasExtension(extensions, VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME) &&
        gfx::HasExtension(extensions,
                          VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
    vk_supports_external_semaphore =
        gfx::HasExtension(extensions,
                          VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME) &&
        gfx::HasExtension(extensions,
                          VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME);
#elif BUILDFLAG(IS_FUCHSIA)
    vk_supports_external_memory =
        gfx::HasExtension(extensions, VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME) &&
        gfx::HasExtension(extensions,
                          VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME);
    vk_supports_external_semaphore =
        gfx::HasExtension(extensions,
                          VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME) &&
        gfx::HasExtension(extensions,
                          VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
#else
    vk_supports_external_memory =
        gfx::HasExtension(extensions, VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME) &&
        gfx::HasExtension(extensions, VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
    vk_supports_external_semaphore =
        gfx::HasExtension(extensions,
                          VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME) &&
        gfx::HasExtension(extensions,
                          VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
#endif
  }
#endif  // BUILDFLAG(ENABLE_VULKAN)

  const bool is_native_vulkan_and_gl =
      vulkan_implementation == gpu::VulkanImplementationName::kNative &&
      !gl::g_current_gl_version->is_swiftshader &&
      !gl::g_current_gl_version->is_angle_swiftshader;

  // Swiftshader GL reports supporting external objects extensions, but doesn't.
  // However, Swiftshader Vulkan and ANGLE can interop using external objects.
  const bool is_swiftshader_vulkan_and_gl =
      vulkan_implementation == gpu::VulkanImplementationName::kSwiftshader &&
      gl::g_current_gl_version->is_angle_swiftshader;

  support_vulkan_external_object_ =
      (is_native_vulkan_and_gl || is_swiftshader_vulkan_and_gl) &&
      gl_supports_memory_object && gl_supports_semaphore &&
      vk_supports_external_memory && vk_supports_external_semaphore;

  support_gl_external_object_flags_ =
      gl::g_current_gl_driver->ext.b_GL_ANGLE_memory_object_flags;

  return true;
}

bool SharedContextState::MakeCurrent(gl::GLSurface* surface, bool needs_gl) {
  if (context_lost()) {
    LOG(ERROR) << "Failed to make current since context is marked as lost";
    return false;
  }

  const bool using_gl = GrContextIsGL() || needs_gl;
  if (using_gl) {
    gl::GLSurface* dont_care_surface =
        last_current_surface_ ? last_current_surface_.get() : surface_.get();
    surface = surface ? surface : dont_care_surface;

    if (!context_->MakeCurrent(surface)) {
      MarkContextLost(error::kMakeCurrentFailed);
      return false;
    }
    last_current_surface_ = surface;
  }

  return !CheckResetStatus(needs_gl);
}

void SharedContextState::ReleaseCurrent(gl::GLSurface* surface) {
  if (!surface)
    surface = last_current_surface_;

  if (surface != last_current_surface_)
    return;

  last_current_surface_ = nullptr;
  if (!context_lost())
    context_->ReleaseCurrent(surface);
}

void SharedContextState::MarkContextLost(error::ContextLostReason reason) {
  if (!context_lost()) {
    scoped_refptr<SharedContextState> prevent_last_ref_drop = this;
    context_lost_reason_ = reason;

    // Notify |context_lost_callback_| and |context_lost_observers_| first,
    // since maybe they still need the GrDirectContext for releasing some skia
    // resources.
    std::move(context_lost_callback_).Run(!device_needs_reset_);
    for (auto& observer : context_lost_observers_)
      observer.OnContextLost();

    // context_state_ could be nullptr for some unittests.
    if (context_state_)
      context_state_->MarkContextLost();

    // Only abandon the GrContext if it is owned by SharedContextState, because
    // the passed in GrContext will be reused.
    // TODO(https://crbug.com/1048692): always abandon GrContext to release all
    // resources when chrome goes into background with low end device.
    if (owned_gr_context_) {
      owned_gr_context_->abandonContext();
      owned_gr_context_.reset();
      gr_context_ = nullptr;
    }
    UpdateSkiaOwnedMemorySize();
  }
}

bool SharedContextState::IsCurrent(gl::GLSurface* surface, bool needs_gl) {
  if (!GrContextIsGL() && !needs_gl)
    return true;
  if (context_lost())
    return false;
  return context_->IsCurrent(surface);
}

// TODO(https://crbug.com/1110357): Account for memory tracked by
// memory_tracker_ and memory_type_tracker_ (e.g. SharedImages allocated in
// SkiaOutputSurfaceImplOnGpu::CopyOutput).
bool SharedContextState::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  if (!gr_context_)
    return true;

  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND) {
    raster::DumpBackgroundGrMemoryStatistics(gr_context_, pmd);
  } else {
    raster::DumpGrMemoryStatistics(gr_context_, pmd, absl::nullopt);
  }

  return true;
}

void SharedContextState::AddContextLostObserver(ContextLostObserver* obs) {
  context_lost_observers_.AddObserver(obs);
}

void SharedContextState::RemoveContextLostObserver(ContextLostObserver* obs) {
  context_lost_observers_.RemoveObserver(obs);
}

void SharedContextState::PurgeMemory(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  if (!gr_context_)
    return;

  // Ensure the context is current before doing any GPU cleanup.
  if (!MakeCurrent(nullptr))
    return;

  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      return;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      // With moderate pressure, clear any unlocked resources.
      sk_surface_cache_.Clear();
      gr_context_->purgeUnlockedResources(true /* scratchResourcesOnly */);
      UpdateSkiaOwnedMemorySize();
      scratch_deserialization_buffer_.resize(
          kInitialScratchDeserializationBufferSize);
      scratch_deserialization_buffer_.shrink_to_fit();
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      // With critical pressure, purge as much as possible.
      sk_surface_cache_.Clear();
      gr_context_->freeGpuResources();
      UpdateSkiaOwnedMemorySize();
      scratch_deserialization_buffer_.resize(0u);
      scratch_deserialization_buffer_.shrink_to_fit();
      break;
  }

  if (transfer_cache_)
    transfer_cache_->PurgeMemory(memory_pressure_level);
}

uint64_t SharedContextState::GetMemoryUsage() {
  UpdateSkiaOwnedMemorySize();
  return memory_tracker_observer_.GetMemoryUsage();
}

void SharedContextState::UpdateSkiaOwnedMemorySize() {
  if (!gr_context_) {
    memory_tracker_observer_.OnMemoryAllocatedChange(CommandBufferId(),
                                                     skia_gr_cache_size_, 0u);
    skia_gr_cache_size_ = 0u;
    return;
  }
  size_t new_size;
  gr_context_->getResourceCacheUsage(nullptr /* resourceCount */, &new_size);
  // Skia does not have a CommandBufferId. PeakMemoryMonitor currently does not
  // use CommandBufferId to identify source, so use zero here to separate
  // prevent confusion.
  memory_tracker_observer_.OnMemoryAllocatedChange(
      CommandBufferId(), skia_gr_cache_size_, static_cast<uint64_t>(new_size));
  skia_gr_cache_size_ = static_cast<uint64_t>(new_size);
}

void SharedContextState::PessimisticallyResetGrContext() const {
  // Calling GrContext::resetContext() is very cheap, so we do it
  // pessimistically. We could dirty less state if skia state setting
  // performance becomes an issue.
  if (gr_context_ && GrContextIsGL())
    gr_context_->resetContext();
}

void SharedContextState::StoreVkPipelineCacheIfNeeded() {
  // GrShaderCache::StoreVkPipelineCacheIfNeeded() must be called only for gpu
  // main thread. Hence using |created_on_compositor_gpu_thread_| to avoid
  // calling it for CompositorGpuThread when DrDc is enabled. See
  // GrShaderCache::StoreVkPipelineCacheIfNeeded for more details.
  if (gr_context_ && gr_shader_cache_ && GrContextIsVulkan() &&
      !created_on_compositor_gpu_thread_) {
    gpu::raster::GrShaderCache::ScopedCacheUse use(gr_shader_cache_,
                                                   kDisplayCompositorClientId);
    gr_shader_cache_->StoreVkPipelineCacheIfNeeded(gr_context_);
  }
}

gl::GLDisplay* SharedContextState::display() {
  return surface_.get()->GetGLDisplay();
}

bool SharedContextState::initialized() const {
  return true;
}

const gles2::ContextState* SharedContextState::GetContextState() {
  if (need_context_state_reset_) {
    // Returning nullptr to force full state restoration by the caller.  We do
    // this because GrContext changes to GL state are untracked in our
    // context_state_.
    return nullptr;
  }
  return context_state_.get();
}

void SharedContextState::RestoreState(const gles2::ContextState* prev_state) {
  PessimisticallyResetGrContext();
  context_state_->RestoreState(prev_state);
  need_context_state_reset_ = false;
}

void SharedContextState::RestoreGlobalState() const {
  PessimisticallyResetGrContext();
  context_state_->RestoreGlobalState(nullptr);
}
void SharedContextState::ClearAllAttributes() const {}

void SharedContextState::RestoreActiveTexture() const {
  PessimisticallyResetGrContext();
}

void SharedContextState::RestoreAllTextureUnitAndSamplerBindings(
    const gles2::ContextState* prev_state) const {
  PessimisticallyResetGrContext();
}

void SharedContextState::RestoreActiveTextureUnitBinding(
    unsigned int target) const {
  PessimisticallyResetGrContext();
}

void SharedContextState::RestoreBufferBinding(unsigned int target) {
  PessimisticallyResetGrContext();
  if (target == GL_PIXEL_PACK_BUFFER) {
    context_state_->UpdatePackParameters();
  } else if (target == GL_PIXEL_UNPACK_BUFFER) {
    context_state_->UpdateUnpackParameters();
  }
  context_state_->api()->glBindBufferFn(target, 0);
}

void SharedContextState::RestoreBufferBindings() const {
  PessimisticallyResetGrContext();
  context_state_->RestoreBufferBindings();
}

void SharedContextState::RestoreFramebufferBindings() const {
  PessimisticallyResetGrContext();
  context_state_->fbo_binding_for_scissor_workaround_dirty = true;
  context_state_->stencil_state_changed_since_validation = true;
}

void SharedContextState::RestoreRenderbufferBindings() {
  PessimisticallyResetGrContext();
  context_state_->RestoreRenderbufferBindings();
}

void SharedContextState::RestoreProgramBindings() const {
  PessimisticallyResetGrContext();
  context_state_->RestoreProgramSettings(nullptr, false);
}

void SharedContextState::RestoreTextureUnitBindings(unsigned unit) const {
  PessimisticallyResetGrContext();
}

void SharedContextState::RestoreVertexAttribArray(unsigned index) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SharedContextState::RestoreAllExternalTextureBindingsIfNeeded() {
  PessimisticallyResetGrContext();
}

QueryManager* SharedContextState::GetQueryManager() {
  return nullptr;
}

absl::optional<error::ContextLostReason> SharedContextState::GetResetStatus(
    bool needs_gl) {
  DCHECK(!context_lost());

  if (gr_context_) {
    // Maybe Skia detected VK_ERROR_DEVICE_LOST.
    if (gr_context_->abandoned()) {
      LOG(ERROR) << "SharedContextState context lost via Skia.";
      return error::kUnknown;
    }

    if (gr_context_->oomed()) {
      LOG(ERROR) << "SharedContextState context lost via Skia OOM.";
      return error::kOutOfMemory;
    }
  }

  // Not using GL.
  if (!GrContextIsGL() && !needs_gl)
    return absl::nullopt;

  // GL is not initialized.
  if (!context_state_)
    return absl::nullopt;

  GLenum error;
  while ((error = context_state_->api()->glGetErrorFn()) != GL_NO_ERROR) {
    if (error == GL_OUT_OF_MEMORY) {
      LOG(ERROR) << "SharedContextState lost due to GL_OUT_OF_MEMORY";
      return error::kOutOfMemory;
    }
    if (error == GL_CONTEXT_LOST_KHR)
      break;
  }
  // Checking the reset status is expensive on some OS/drivers
  // (https://crbug.com/1090232). Rate limit it.
  constexpr base::TimeDelta kMinCheckDelay = base::Milliseconds(5);
  base::Time now = base::Time::Now();
  if (!disable_check_reset_status_throttling_for_test_ &&
      now < last_gl_check_graphics_reset_status_ + kMinCheckDelay) {
    return absl::nullopt;
  }
  last_gl_check_graphics_reset_status_ = now;

  GLenum driver_status = context()->CheckStickyGraphicsResetStatus();
  if (driver_status == GL_NO_ERROR)
    return absl::nullopt;
  LOG(ERROR) << "SharedContextState context lost via ARB/EXT_robustness. Reset "
                "status = "
             << gles2::GLES2Util::GetStringEnum(driver_status);

  switch (driver_status) {
    case GL_GUILTY_CONTEXT_RESET_ARB:
      return error::kGuilty;
    case GL_INNOCENT_CONTEXT_RESET_ARB:
      return error::kInnocent;
    case GL_UNKNOWN_CONTEXT_RESET_ARB:
      return error::kUnknown;
    default:
      NOTREACHED();
      break;
  }
  return absl::nullopt;
}

bool SharedContextState::CheckResetStatus(bool need_gl) {
  DCHECK(!context_lost());
  DCHECK(!device_needs_reset_);

  auto status = GetResetStatus(need_gl);
  if (status.has_value()) {
    device_needs_reset_ = true;
    MarkContextLost(status.value());
    return true;
  }
  return false;
}

void SharedContextState::ScheduleGrContextCleanup() {
  if (gr_cache_controller_)
    gr_cache_controller_->ScheduleGrContextCleanup();
}

}  // namespace gpu
